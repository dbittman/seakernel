/*
 * This implementation of the Rijndael cipher was obtained from:
 * http://www.efgh.com/software/rijndael.htm
 * Author: Philip J. Erdelsky (pje@acm.org)
 * Code released to public domain, according to Web site
 *
 * Minor changes made by:
 * Ethan L. Miller (elm@cs.ucsc.edu)
 * 
 * Adapted for use in SeaOS by Daniel Bittman
 */
#ifndef H__RIJNDAEL
#define H__RIJNDAEL

#include <sea/types.h>

int rijndaelSetupEncrypt(uint32_t *rk, const uint8_t *key,
  int keybits);
int rijndaelSetupDecrypt(uint32_t *rk, const uint8_t *key,
  int keybits);
void rijndaelEncrypt(const uint32_t *rk, int nrounds,
  const uint8_t plaintext[16], uint8_t ciphertext[16]);
void rijndaelDecrypt(const uint32_t *rk, int nrounds,
  const uint8_t ciphertext[16], uint8_t plaintext[16]);

#define KEYLENGTH(keybits) ((keybits)/8)
//#define RKLENGTH(keybits)  ((keybits)/8+28)
#define NROUNDS(keybits)   ((keybits)/32+6)

#define RKBYTES(bits) (((bits)/8+28) * 4)

#endif

