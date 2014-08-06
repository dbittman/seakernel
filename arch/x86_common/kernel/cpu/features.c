#include <sea/kernel.h>
#include <sea/cpu/processor.h>
#include <sea/asm/system.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif
void x86_cpu_init_fpu(cpu_t *me)
{
	if(me->cpuid.features_edx & 0x01)
	{
		printk(KERN_EVERY, "\tFPU...");
		asm("finit;");  
		me->flags |= CPU_FPU;
		unsigned long cr0;
		asm("mov %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		cr0 |= 0x20;
		asm("mov %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
	}
}

void x86_cpu_init_sse(cpu_t *me)
{
	unsigned long cr0, cr4;
	if(me->cpuid.features_edx & 0x06000001) /* test for SSE2, SSE, and FPU */
	{
		printk(KERN_EVERY, "SSE...");
		asm("mov %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		asm("mov %%cr4, %0;":"=r"(cr4)); /* store CR4 */
		cr0 &= ~CR0_EM; /* disable FPU emulation */
		cr0 |= CR0_MP;  /* set MP bit */
		cr4 |= (CR4_OSFXSR | CR4_OSXMMEXCPT); /* set the bits to enable FPU SAVE/RESTORE and XMM registers */
		asm("mov %0, %%cr4;"::"r"(cr4)); /* restore CR4 */
		asm("mov %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
		me->flags |= CPU_SSE;
	}
	if(me->cpuid.features_edx & (1 << 24))
		me->flags |= CPU_FXSAVE;
}
