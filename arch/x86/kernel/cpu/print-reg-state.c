//print out register values when hardware exception in the kernel

#include <sea/cpu/registers-x86.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

//print out flags set in eflags register
static void print_eflags(registers_t *regs){
	uint32_t flags=regs->eflags;
	/*reserved bits correspond to "r"*/
	char *flag_array[]={"CF","r","PF","r","AF","r","ZF","SF",\
			    "TF", "IF", "DF", "OF", "IOPL(12)",\
			    "IOPL(13)", "NT", "r", "RF", "VM", \
			    "AC", "VIF", "VIP", "ID"};
	uint32_t mask=1;
	int i=0;
	printk_safe(5, "eflags: 0x%x \t[", flags);
	for(i=0;i<22;++i){
		/* print flag if currently examined bit is 1 and not reserved*/
		if((mask&flags)&&(strcmp(flag_array[i],"r")!=0)){
			printk_safe(5, " %s,",flag_array[i]);
		}
		flags>>=1;
		
	}
	printk_safe(5, "]\n");
}

void arch_cpu_print_reg_state(registers_t *regs){
	printk_safe(5, "ds:0x%x\ncs:0x%x\nss:0x%x\n",regs->ds,regs->cs, regs->ss);
	printk_safe(5, "edi:0x%x\nesi:0x%x\nebp:0x%x\nesp:0x%x\neip:0x%x\n",regs->edi,regs->esi,regs->ebp, regs->esp, regs->eip);
	printk_safe(5, "eax:0x%x\nebx:0x%x\necx:0x%x\nedx:0x%x\n",regs->eax, regs->ebx, regs->ecx, regs->edx);
	printk_safe(5, "int_no:0x%x\nerr_code:0x%x\n", regs->int_no,regs->err_code);
	printk_safe(5, "useresp:0x%x\n", regs->useresp);
	print_eflags(regs);
}

