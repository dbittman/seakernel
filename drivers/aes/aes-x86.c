#include <kernel.h>
#include <module.h>
#include <config.h>

#if CONFIG_ARCH == TYPE_ARCH_X86 || CONFIG_ARCH == TYPE_ARCH_X86_64

void aes_x86_128_encrypt()
{
	/* set up the data */
	unsigned char *data;
	asm("movdqa (%0), %%xmm7" :: "r"(data));
	asm(" \
		pxor %xmm7, %xmm0");
}

#endif
