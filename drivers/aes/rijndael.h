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

int rijndaelSetupEncrypt(u32 *rk, const u8 *key,
  int keybits);
int rijndaelSetupDecrypt(u32 *rk, const u8 *key,
  int keybits);
void rijndaelEncrypt(const u32 *rk, int nrounds,
  const u8 plaintext[16], u8 ciphertext[16]);
void rijndaelDecrypt(const u32 *rk, int nrounds,
  const u8 ciphertext[16], u8 plaintext[16]);

#define KEYLENGTH(keybits) ((keybits)/8)
//#define RKLENGTH(keybits)  ((keybits)/8+28)
#define NROUNDS(keybits)   ((keybits)/32+6)

#define RKBYTES(bits) (((bits)/8+28) * 4)

#endif

