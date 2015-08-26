#include <sea/cpu/processor.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/tty/terminal.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif
void x86_cpu_init_fpu(struct cpu *me)
{
	if(me->cpuid.features_edx & 0x01)
	{
		__asm__ __volatile__ ("finit;");  
		unsigned long cr0;
		__asm__ __volatile__ ("mov %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		cr0 |= 0x20;
		__asm__ __volatile__ ("mov %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
	}
}

void x86_cpu_init_sse(struct cpu *me)
{
	unsigned long cr0, cr4;
	if(me->cpuid.features_edx & 0x06000001) /* test for SSE2, SSE, and FPU */
	{
		__asm__ __volatile__ ("mov %%cr0, %0;":"=r"(cr0)); /* store CR0 */
		__asm__ __volatile__ ("mov %%cr4, %0;":"=r"(cr4)); /* store CR4 */
		cr0 &= ~CR0_EM; /* disable FPU emulation */
		cr0 |= CR0_MP;  /* set MP bit */
		cr4 |= (CR4_OSFXSR | CR4_OSXMMEXCPT); /* set the bits to enable FPU SAVE/RESTORE and XMM registers */
		__asm__ __volatile__ ("mov %0, %%cr4;"::"r"(cr4)); /* restore CR4 */
		__asm__ __volatile__ ("mov %0, %%cr0;"::"r"(cr0)); /* restore CR0 */
	}
}

