#include <sea/kernel.h>
#include <sea/mm/vmm.h>

/* handle a page fault. Convert arch-dep things into generic
 * kernel stuff (the error codes), and get the address. */
void arch_mm_page_fault_handle(registers_t *regs, int int_no, int flags)
{
	assert(regs);
	addr_t cr2, err_code = regs->err_code;
	__asm__ volatile ("mov %%cr2, %0" : "=r" (cr2));
	__asm__ volatile ("mov $0, %%eax; mov %%eax, %%cr2" ::: "eax");
	int pf_error=0;
	if(!(err_code & 1))
		pf_error |= PF_CAUSE_NONPRESENT;
	if(err_code & (1 << 1))
		pf_error |= PF_CAUSE_WRITE;
	else
		pf_error |= PF_CAUSE_READ;
	if(err_code & (1 << 2))
		pf_error |= PF_CAUSE_USER;
	else
		pf_error |= PF_CAUSE_SUPER;
	if(err_code & (1 << 3))
		pf_error |= PF_CAUSE_RSVD;
	if(err_code & (1 << 4))
		pf_error |= PF_CAUSE_IFETCH;

	mm_page_fault_handler(regs, cr2, pf_error);
}

