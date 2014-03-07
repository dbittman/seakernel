/*
 * This implementation of the Rijndael cipher was obtained from:
 * http://www.efgh.com/software/rijndael.htm
 * Author: Philip J. Erdelsky (pje@acm.org)
 * Code released to public domain, according to Web site
 *
 * Minor changes made by:
 * Ethan L. Miller (elm@cs.ucsc.edu)
 */
#ifndef H__RIJNDAEL
#define H__RIJNDAEL

#include <types.h>

int rijndaelSetupEncrypt(uint32_t *rk, const unsigned char *key,
  int keybits);
int rijndaelSetupDecrypt(uint32_t *rk, const unsigned char *key,
  int keybits);
void rijndaelEncrypt(const uint32_t *rk, int nrounds,
  const unsigned char plaintext[16], unsigned char ciphertext[16]);
void rijndaelDecrypt(const uint32_t *rk, int nrounds,
  const unsigned char ciphertext[16], unsigned char plaintext[16]);

#define KEYLENGTH(keybits) ((keybits)/8)
//#define RKLENGTH(keybits)  ((keybits)/8+28)
#define NROUNDS(keybits)   ((keybits)/32+6)

#define RKBYTES(bits) (((bits)/8+28) * 4)

#endif

