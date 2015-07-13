/*print out register values when hardware exception occurs in kernel*/

#include <sea/cpu/registers-x86_64.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

//print out flags set in eflags register
static void print_eflags(registers_t *regs){
	uint64_t flags=regs->eflags;
	/*reserved bits correspond to "r"*/
	char *flag_array[]={"CF","r","PF","r","AF","r","ZF","SF",\
			    "TF", "IF", "DF", "OF", "IOPL(12)",\
			    "IOPL(13)", "NT", "r", "RF", "VM", \
			    "AC", "VIF", "VIP", "ID"};
	uint64_t mask=1;
	int i=0;
	kprintf("eflags: 0x%x \t[", flags);
	for(i=0;i<22;++i){
		/* print flag if currently examined bit is 1 and not reserved*/
		if((mask&flags)&&(strcmp(flag_array[i],"r")!=0)){
			kprintf(" %s,",flag_array[i]);
		}
		/*examine next bit*/
		flags>>=1;

	}
	kprintf("]\n");
}

void arch_cpu_print_reg_state(registers_t *regs){
	kprintf("ds:0x%x\ncs:0x%x\nss:0x%x\n",regs->ds, regs->cs, regs->ss);
	kprintf("r15:0x%x\nr14:0x%x\nr13:0x%x\nr12:0x%x\nr11:0x%x\nr10:0x%x\nr9:0x%x\nr8:0x%x\n", regs->r15, regs->r14, regs->r13, regs->r12, regs->r11, regs->r10, regs->r9, regs->r8);
	kprintf("eip:0x%x\nrbp:0x%x\nrsi:0x%x\nrdi:0x%x\n", regs->eip, regs->rbp, regs->rsi, regs->rdi);
	kprintf("rax:0x%x\nrbx:0x%x\nrcx:0x%x\nrdx:0x%x\n",regs->rax, regs->rbx, regs->rcx, regs->rdx); 
	kprintf("int_no:0x%x\nerr_code:0x%x\n", regs->int_no, regs->err_code);
	kprintf("useresp:0x%x\n", regs->useresp);
	print_eflags(regs);
}

