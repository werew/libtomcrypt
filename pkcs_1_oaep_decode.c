/* LibTomCrypt, modular cryptographic library -- Tom St Denis
 *
 * LibTomCrypt is a library that provides various cryptographic
 * algorithms in a highly modular and flexible manner.
 *
 * The library is free for all purposes without any express
 * guarantee it works.
 *
 * Tom St Denis, tomstdenis@iahu.ca, http://libtomcrypt.org
 */
#include "mycrypt.h"

/* OAEP Padding for PKCS #1 -- Tom St Denis */

#ifdef PKCS_1

int pkcs_1_oaep_decode(const unsigned char *msg,    unsigned long msglen,
                       const unsigned char *lparam, unsigned long lparamlen,
                             unsigned long modulus_bitlen, int hash_idx,
                             unsigned char *out,    unsigned long *outlen,
                             int           *res)
{
   unsigned char *DB, *seed, *mask;
   unsigned long hLen, x, y, modulus_len;
   int           err;

   _ARGCHK(msg    != NULL);
   _ARGCHK(out    != NULL);
   _ARGCHK(outlen != NULL);
   _ARGCHK(res    != NULL);

   /* default to invalid packet */
   *res = 0;
   
   /* test valid hash */
   if ((err = hash_is_valid(hash_idx)) != CRYPT_OK) { 
      return err;
   }
   hLen        = hash_descriptor[hash_idx].hashsize;
   modulus_len = (modulus_bitlen >> 3) + (modulus_bitlen & 7 ? 1 : 0);

   /* allocate ram for DB/mask/salt of size modulus_len */
   DB   = XMALLOC(modulus_len);
   mask = XMALLOC(modulus_len);
   seed = XMALLOC(modulus_len);
   if (DB == NULL || mask == NULL || seed == NULL) {
      if (DB != NULL) {
         XFREE(DB);
      }
      if (mask != NULL) {
         XFREE(mask);
      }
      if (seed != NULL) {
         XFREE(seed);
      }
      return CRYPT_MEM;
   }


   /* test message size */
   if (msglen != modulus_len) {
      err = CRYPT_PK_INVALID_SIZE;
      goto __ERR;
   }

   /* ok so it's now in the form
  
      0x00  || maskedseed || maskedDB 
  
       1    ||   hLen     ||  modulus_len - hLen - 1
   
    */

   /* must have leading 0x00 byte */
   if (msg[0] != 0x00) {
      err = CRYPT_OK;
      goto __ERR;
   }

   /* now read the masked seed */
   for (x = 1, y = 0; y < hLen; y++) {
      seed[y] = msg[x++];
   }

   /* now read the masked DB */
   for (y = 0; y < modulus_len - hLen - 1; y++) {
      DB[y] = msg[x++];
   }

   /* compute MGF1 of maskedDB (hLen) */ 
   if ((err = pkcs_1_mgf1(DB, modulus_len - hLen - 1, hash_idx, mask, hLen)) != CRYPT_OK) {
      goto __ERR;
   }

   /* XOR against seed */
   for (y = 0; y < hLen; y++) {
      seed[y] ^= mask[y];
   }

   /* compute MGF1 of seed (k - hlen - 1) */
   if ((err = pkcs_1_mgf1(seed, hLen, hash_idx, mask, modulus_len - hLen - 1)) != CRYPT_OK) {
      goto __ERR;
   }

   /* xor against DB */
   for (y = 0; y < (modulus_len - hLen - 1); y++) {
       DB[y] ^= mask[y]; 
   }

   /* now DB == lhash || PS || 0x01 || M, PS == k - mlen - 2hlen - 2 zeroes */

   /* compute lhash and store it in seed [reuse temps!] */
   x = modulus_len;
   if (lparam != NULL) {
      if ((err = hash_memory(hash_idx, lparam, lparamlen, seed, &x)) != CRYPT_OK) {
         goto __ERR;
      }
   } else {
      /* can't pass hash_memory a NULL so use DB with zero length */
      if ((err = hash_memory(hash_idx, DB, 0, seed, &x)) != CRYPT_OK) {
         goto __ERR;
      }
   }

   /* compare the lhash'es */
   if (memcmp(seed, DB, hLen) != 0) {
      err = CRYPT_OK;
      goto __ERR;
   }

   /* now zeroes before a 0x01 */
   for (x = hLen; x < (modulus_len - hLen - 1) && DB[x] == 0x00; x++) {
      /* step... */
   }

   /* error out if wasn't 0x01 */
   if (x == (modulus_len - hLen - 1) || DB[x] != 0x01) {
      err = CRYPT_OK;
      goto __ERR;
   }

   /* rest is the message (and skip 0x01) */
   if (msglen - ++x > *outlen) {
      err = CRYPT_BUFFER_OVERFLOW;
      goto __ERR;
   }

   /* copy message */
   *outlen = (modulus_len - hLen - 1) - x;
   for (y = 0; x != (modulus_len - hLen - 1); ) {
       out[y++] = DB[x++];
   }

   /* valid packet */
   *res = 1;

   err = CRYPT_OK;
__ERR:
#ifdef CLEAN_STACK
   zeromem(DB,   modulus_len);
   zeromem(seed, modulus_len);
   zeromem(mask, modulus_len);
#endif

   XFREE(seed);
   XFREE(mask);
   XFREE(DB);

   return err;
}

#endif /* PKCS_1 */