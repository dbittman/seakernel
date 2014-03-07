#include <kernel.h>
#include <module.h>
#include <sea/cpu/processor.h>
#include <sea/loader/symbol.h>
#include "rijndael.h"

#if CONFIG_ARCH == TYPE_ARCH_X86_64
void aes_x86_128_encrypt_block(unsigned char *plaintext, unsigned char *ciphertext, unsigned char *round_keys);
void aes_x86_128_decrypt_block(unsigned char *ciphertext, unsigned char *plaintext, unsigned char *dec_round_keys);
void aes_x86_128_key_expand(unsigned char *key, unsigned char *round_keys);
void aes_x86_128_key_inv_transform(unsigned char *round_keys, unsigned char *dec_round_keys);

int has_intel_aes = 0;

#endif

int aes_setup_encrypt(unsigned char *key, unsigned char *round_keys, int keybits)
{
	if(keybits != 128)
		panic(0, "AES doesn't support non-128 keylength");
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	if(has_intel_aes) {
		aes_x86_128_key_expand(key, round_keys);
		return 10;
	}
#endif
	return rijndaelSetupEncrypt((uint32_t *)round_keys, key, keybits);
}

int aes_setup_decrypt(unsigned char *key, unsigned char *round_keys, int keybits)
{
	if(keybits != 128)
		panic(0, "AES doesn't support non-128 keylength");
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	if(has_intel_aes) {
		unsigned char tmp_round_keys[RKBYTES(keybits)];
		aes_x86_128_key_expand(key, tmp_round_keys);
		aes_x86_128_key_inv_transform(tmp_round_keys, round_keys);
		return 10;
	}
#endif
	return rijndaelSetupDecrypt((uint32_t *)round_keys, key, keybits);
}

void aes_encrypt_block(unsigned char *round_keys, int nrounds, unsigned char *plaintext, unsigned char *ciphertext)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	if(has_intel_aes) {
		aes_x86_128_encrypt_block(plaintext, ciphertext, round_keys);
		return;
	}
#endif
	rijndaelEncrypt((uint32_t *)round_keys, nrounds, plaintext, ciphertext);
}

void aes_decrypt_block(unsigned char *dec_round_keys, int nrounds, unsigned char *ciphertext, unsigned char *plaintext)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	if(has_intel_aes) {
		aes_x86_128_decrypt_block(ciphertext, plaintext, dec_round_keys);
		return;
	}
#endif
	rijndaelDecrypt((uint32_t *)dec_round_keys, nrounds, ciphertext, plaintext);
}

int module_install()
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	has_intel_aes = primary_cpu->cpuid.features_ecx & (1 << 25) ? 1 : 0;
	if(has_intel_aes)
		printk(0, "[aes]: intel hardware assisted AES instructions are supported\n");
#endif
	
	loader_add_kernel_symbol(aes_setup_decrypt);
	loader_add_kernel_symbol(aes_setup_encrypt);
	loader_add_kernel_symbol(aes_decrypt_block);
	loader_add_kernel_symbol(aes_encrypt_block);
		
	/*
	unsigned char rk[RKBYTES(128)];
	unsigned char key[KEYLENGTH(128)];
	unsigned char dec_rk[RKBYTES(128)];
	unsigned char plain[16];
	unsigned char ciph[16];
	unsigned char dec[16];
	
	memset(key, 0, 16);
	key[0]=1;
	memset(plain, 0, 16);
	plain[0] = 2;
	
	int en = aes_setup_encrypt(key, rk, 128);
	int dn = aes_setup_decrypt(key, dec_rk, 128);
	
	aes_encrypt_block(rk, en, plain, ciph);
	aes_decrypt_block(dec_rk, dn, ciph, dec);
	
	int i;
	
	kprintf("\nplaintext : "); for(i=0;i<16;i++) kprintf("%02x", plain[i]);
	kprintf("\nciphertext: "); for(i=0;i<16;i++) kprintf("%02x", ciph[i]);
	kprintf("\ndecrypted : "); for(i=0;i<16;i++) kprintf("%02x", dec[i]);
	kprintf("\n");
	
	for(;;);
	*/
	return 0;
}

int module_exit()
{
	loader_remove_kernel_symbol("aes_setup_decrypt");
	loader_remove_kernel_symbol("aes_setup_encrypt");
	loader_remove_kernel_symbol("aes_decrypt_block");
	loader_remove_kernel_symbol("aes_encrypt_block");
	return 0;
}
