#include <kernel.h>
#include <module.h>
#include <sea/cpu/processor.h>
#include "rijndael.h"
#if CONFIG_ARCH == TYPE_ARCH_X86_64
void aes_x86_128_encrypt_block(unsigned char *plaintext, unsigned char *ciphertext, unsigned char *round_keys);
void aes_x86_128_decrypt_block(unsigned char *ciphertext, unsigned char *plaintext, unsigned char *dec_round_keys);
void aes_x86_128_key_expand(unsigned char *key, unsigned char *round_keys);
void aes_x86_128_key_inv_transform(unsigned char *round_keys, unsigned char *dec_round_keys);

int has_intel_aes = 0, has_avx = 0;

#endif

int module_install()
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	has_intel_aes = primary_cpu->cpuid.features_ecx & (1 << 25) ? 1 : 0;
	has_avx = primary_cpu->cpuid.features_ecx & (1 << 28) ? 1 : 0;
	kprintf("%x\n", primary_cpu->cpuid.features_ecx);
	if(has_intel_aes && has_avx)
		printk(0, "[aes]: intel hardware assisted AES instructions supported\n");
#endif
	
	unsigned char key[16];
	unsigned char round_keys[176];
	unsigned char dec_keys[176];
	unsigned char plaintext[16];
	unsigned char ciph[16];
	memset(key, 0, 16);
	memset(plaintext, 0, 16);
	plaintext[0]=2;
	key[0] = 1;
	key[1] = 0;
	
	kprintf("KEY EXP (%d)\n", RKLENGTH(128));
	
	aes_x86_128_key_expand(key, round_keys);
	uint32_t *q = (uint32_t *)round_keys;
	int i;
	for(i=0;i<44;i++)
		kprintf("%08x ", q[i]);
	
	uint32_t rk[44];
	
	int n = rijndaelSetupEncrypt(rk, key, 128);
	kprintf("\nNR: %d\n", n);
	for(i=0;i<44;i++)
		kprintf("%08x ", rk[i]);
	kprintf("\n\n");
	aes_x86_128_encrypt_block(plaintext, ciph, round_keys);
	for(i=0;i<16;i++)
		printk(5, "%02x", ciph[i]);
	
	rijndaelEncrypt(rk, n, plaintext, ciph);
	kprintf("\n");
	for(i=0;i<16;i++)
		printk(5, "%02x", ciph[i]);
	
	unsigned char buf[16];
	
	aes_x86_128_key_inv_transform(round_keys, dec_keys);
	
	aes_x86_128_decrypt_block(ciph, buf, dec_keys);
	kprintf("\n\n");
	for(i=0;i<16;i++)
		printk(5, "%02x", buf[i]);
	
	
	for(;;);
	
	return 0;
}

int module_exit()
{
	
	return 0;
}
