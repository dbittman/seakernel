#include <kernel.h>
#include <module.h>
#include <config.h>

#if CONFIG_ARCH == TYPE_ARCH_X86_64

void aes_x86_128_encrypt_block(unsigned char *plaintext, unsigned char *ciphertext, unsigned char *round_keys)
{
	/* load the data */
	asm("movdqu (%0), %%xmm15" :: "r"(plaintext));
	/* load the round keys */
	asm("movdqu (%0), %%xmm0" :: "r"(round_keys));
	asm("movdqu (%0), %%xmm1" :: "r"(round_keys + 16));
	asm("movdqu (%0), %%xmm2" :: "r"(round_keys + 32));
	asm("movdqu (%0), %%xmm3" :: "r"(round_keys + 48));
	asm("movdqu (%0), %%xmm4" :: "r"(round_keys + 64));
	asm("movdqu (%0), %%xmm5" :: "r"(round_keys + 80));
	asm("movdqu (%0), %%xmm6" :: "r"(round_keys + 96));
	asm("movdqu (%0), %%xmm7" :: "r"(round_keys + 112));
	asm("movdqu (%0), %%xmm8" :: "r"(round_keys + 128));
	asm("movdqu (%0), %%xmm9" :: "r"(round_keys + 144));
	asm("movdqu (%0), %%xmm10" :: "r"(round_keys + 160));
	/* ...and do the rounds with them */
	asm("pxor %xmm0, %xmm15; \
	     aesenc %xmm1, %xmm15;\
	     aesenc %xmm2, %xmm15;\
	     aesenc %xmm3, %xmm15;\
	     aesenc %xmm4, %xmm15;\
	     aesenc %xmm5, %xmm15;\
	     aesenc %xmm6, %xmm15;\
	     aesenc %xmm7, %xmm15;\
	     aesenc %xmm8, %xmm15;\
	     aesenc %xmm9, %xmm15;\
	     aesenclast %xmm10, %xmm15;");
	/* copy the data out of xmm15 */
	asm("movdqu %%xmm15, (%0)" :: "r"(ciphertext));
}

void aes_x86_128_decrypt_block(unsigned char *ciphertext, unsigned char *plaintext, unsigned char *dec_round_keys)
{
	/* load the data */
	asm("movdqu (%0), %%xmm15" :: "r"(ciphertext));
	/* load the round keys */
	asm("movdqu (%0), %%xmm10" :: "r"(dec_round_keys + 160));
	asm("movdqu (%0), %%xmm9" :: "r"(dec_round_keys + 144));
	asm("movdqu (%0), %%xmm8" :: "r"(dec_round_keys + 128));
	asm("movdqu (%0), %%xmm7" :: "r"(dec_round_keys + 112));
	asm("movdqu (%0), %%xmm6" :: "r"(dec_round_keys + 96));
	asm("movdqu (%0), %%xmm5" :: "r"(dec_round_keys + 80));
	asm("movdqu (%0), %%xmm4" :: "r"(dec_round_keys + 64));
	asm("movdqu (%0), %%xmm3" :: "r"(dec_round_keys + 48));
	asm("movdqu (%0), %%xmm2" :: "r"(dec_round_keys + 32));
	asm("movdqu (%0), %%xmm1" :: "r"(dec_round_keys + 16));
	asm("movdqu (%0), %%xmm0" :: "r"(dec_round_keys));
	/* ...and do the rounds with them */
	asm("pxor %xmm10, %xmm15; \
	     aesdec %xmm9, %xmm15;\
	     aesdec %xmm8, %xmm15;\
	     aesdec %xmm7, %xmm15;\
	     aesdec %xmm6, %xmm15;\
	     aesdec %xmm5, %xmm15;\
	     aesdec %xmm4, %xmm15;\
	     aesdec %xmm3, %xmm15;\
	     aesdec %xmm2, %xmm15;\
	     aesdec %xmm1, %xmm15;\
	     aesdeclast %xmm0, %xmm15;");
	/* copy the data out of xmm15 */
	asm("movdqu %%xmm15, (%0)" :: "r"(plaintext));
}

void aes_x86_128_key_expand(unsigned char *key, unsigned char *round_keys)
{
	/* This code is adopted from Intel's documents on AES instructions, 
	 * with clarifications based on the linux AES-NI code. */
	asm(" \
	     movdqu (%1), %%xmm0;\
	     movdqu %%xmm0, (%0);\
	     lea 0x10(%0), %%rcx;\
	     pxor %%xmm4, %%xmm4; \
	     \
	     aeskeygenassist $0x1, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x2, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x4, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x8, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x10, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x20, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x40, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x80, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x1b, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     aeskeygenassist $0x36, %%xmm0, %%xmm1; \
	     call key_expansion_128; \
	     jmp __end;\
	     \
	     key_expansion_128: \
	     pshufd $0xff, %%xmm1, %%xmm1; \
	     shufps $0b00010000, %%xmm0, %%xmm4; \
	     pxor %%xmm4, %%xmm0; \
	     shufps $0b10001100, %%xmm0, %%xmm4; \
	     pxor %%xmm4, %%xmm0; \
	     pxor %%xmm1, %%xmm0; \
	     movdqu %%xmm0, (%%rcx); \
	     add $0x10, %%rcx; \
	     ret; \
	     __end:\
	     " :: "r"(round_keys), "r"(key) : "rcx");
}

void aes_x86_128_key_inv_transform(unsigned char *round_keys, unsigned char *dec_round_keys)
{
	/* call the inversion instruction on most of the keys */
	asm(" \
	     mov %0, %%rdx;                 \
	     mov %1, %%rax;                 \
	     movdqu (%%rdx), %%xmm1;        \
	     movdqu %%xmm1, (%%rax);        \
	     add $0x10, %%rdx;              \
	     add $0x10, %%rax;              \
	                                    \
	     mov $9, %%ecx;                 \
	     repeat:                        \
	         movdqu (%%rdx), %%xmm1;    \
	         aesimc %%xmm1, %%xmm1;     \
	         movdqu %%xmm1, (%%rax);    \
	         add $0x10, %%rdx;          \
	         add $0x10, %%rax;          \
	     loop repeat;                   \
	                                    \
	     movdqu (%%rdx), %%xmm1;        \
	     movdqu %%xmm1, (%%rax);        \
	     " :: "r"(round_keys), "r"(dec_round_keys) : "rdx", "rax");
}

#endif
