#include <kernel.h>
#include <cpu.h>

void setup_fpu(cpu_t *me)
{
	if(me->cpuid.features_edx & 0x01)
	{
		printk(KERN_EVERY, "\tFPU...");
		asm("finit;");  
		me->flags |= CPU_FPU;
		uint32_t cr0, cr4;
		asm("movl %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		cr0 |= 0x20;
		asm("movl %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
	}
}

void init_sse(cpu_t *me)
{
	uint32_t cr0, cr4;
	if(me->cpuid.features_edx & 0x06000001) /* test for SSE2, SSE, and FPU */
	{
		printk(KERN_EVERY, "SSE...");
		asm("movl %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		asm("movl %%cr4, %0;":"=r"(cr4)); /* store CR4 */
		cr0 &= ~CR0_EM; /* disable FPU emulation */
		cr0 |= CR0_MP;  /* set MP bit */
		cr4 |= (CR4_OSFXSR | CR4_OSXMMEXCPT); /* set the bits to enable FPU SAVE/RESTORE and XMM registers */
		asm("movl %0, %%cr4;"::"r"(cr4)); /* restore CR4 */
		asm("movl %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
		me->flags |= CPU_SSE;
	}   
}
