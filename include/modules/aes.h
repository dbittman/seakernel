#ifndef __MOD_AES_H
#define __MOD_AES_H
#include <config.h>
#if CONFIG_MODULE_AES

#include <types.h>

#define KEYLENGTH(keybits) ((keybits)/8)
#define RKBYTES(bits) (((bits)/8+28) * 4)

int aes_setup_encrypt(unsigned char *key, unsigned char *round_keys, int keybits);
int aes_setup_decrypt(unsigned char *key, unsigned char *round_keys, int keybits);
void aes_encrypt_block(unsigned char *round_keys, int nrounds, unsigned char *plaintext, unsigned char *ciphertext);
void aes_decrypt_block(unsigned char *dec_round_keys, int nrounds, unsigned char *ciphertext, unsigned char *plaintext);

#endif
#endif
