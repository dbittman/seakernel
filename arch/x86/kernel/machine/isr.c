/* Privides functions for interrupt handling. */
#include <kernel.h>
#include <isr.h>
#include <asm/system.h>
#include <task.h>
#include <cpu.h>
#include <elf.h>
#include <atomic.h>

extern char *exception_messages[];
isr_t interrupt_handlers[256][256][2];
unsigned int stage2_count[256];
volatile long int_count[256];
mutex_t isr_lock;
char interrupt_controller=0;
/* Interrupt handlers are stored in linked lists 
 * (allows 'infinite' number of them). But we 
 * need kmalloc to do this. If we have kmalloc 
 * (aka, we have MM), we do the linked list thing. 
 * Otherwise we just set 1 interrupt handler.
 * 
 * We make interrupt_handlers a local array so 
 * that we can use it without kmalloc.
 */
int register_interrupt_handler(u8int num, isr_t stage1_handler, isr_t stage2_handler)
{
	mutex_acquire(&isr_lock);
	int i;
	for(i=0;i<256;i++)
	{
		if(!interrupt_handlers[num][i][0] && !interrupt_handlers[num][i][1])
		{
			interrupt_handlers[num][i][0] = stage1_handler;
			interrupt_handlers[num][i][1] = stage2_handler;
			break;
		}
	}
	mutex_release(&isr_lock);
	if(i == 256) panic(0, "ran out of interrupt handlers");
	return i;
}

void unregister_interrupt_handler(u8int n, int id)
{
	mutex_acquire(&isr_lock);
	if(!interrupt_handlers[n][id][0] && !interrupt_handlers[n][id][1])
		panic(0, "tried to unregister an empty interrupt handler");
	interrupt_handlers[n][id][0] = interrupt_handlers[n][id][1] = 0;
	mutex_release(&isr_lock);
}

void kernel_fault(int fuckoff)
{
	kprintf("Kernel Exception #%d: ", fuckoff);
	printk(5, "Occured in task %d during systemcall %d (F=%d).\n",
			current_task->pid, current_task->system, current_task->flag);
	
	panic(0, exception_messages[fuckoff]);
}

const char *special_names(int i)
{
	if(i == 0x80)
		return "(syscall)";
	switch(i) {
		case IPI_DEBUG:
			return "(debug)";
		case IPI_SHUTDOWN:
			return "(shutdown)";
		case IPI_PANIC:
			return "(panic)";
		case IPI_SCHED:
			return "(schedule)";
		case IPI_TLB:
			return "(tlb rst)";
		case IPI_TLB_ACK:
			return "(tlb ack)";
	}
	return "\t";
}

void faulted(int fuckoff)
{
	if(current_task == kernel_task || !current_task || current_task->system)
	{
		kernel_fault(fuckoff);
	} else
	{
		printk(5, "%s occured in task %d (F=%d): He's dead, Jim.\n", 
				exception_messages[fuckoff], current_task->pid, current_task->flag);
		switch(fuckoff)
		{
			case 0: case 5: case 6: case 13:
				send_signal(current_task->pid, SIGILL);
				break;
			case 1: case 3: case 4:
				send_signal(current_task->pid, SIGTRAP);
				break;
			case 8: case 18:
				send_signal(current_task->pid, SIGABRT);
				break;
			default:
				kill_task(current_task->pid);
				break;
		}
		schedule();
	}
}

void ack_pic(int n)
{
	assert(interrupt_controller == IOINT_PIC);
	if(n >= IRQ0 && n < IRQ15) {
		if (n >= 40)
			outb(0xA0, 0x20);
		outb(0x20, 0x20);
	}
}

void ipi_handler(volatile registers_t regs)
{
	set_int(0);
	add_atomic(&int_count[regs.int_no], 1);
#if CONFIG_SMP
	/* delegate to the proper handler, in ipi.c */
	switch(regs.int_no) {
		case IPI_DEBUG:
		case IPI_SHUTDOWN:
		case IPI_PANIC:
			handle_ipi_cpu_halt(regs);
			break;
		case IPI_SCHED:
			handle_ipi_reschedule(regs);
			break;
		case IPI_TLB:
			handle_ipi_tlb(regs);
			break;
		case IPI_TLB_ACK:
			handle_ipi_tlb_ack(regs);
			break;
		default:
			panic(PANIC_NOSYNC, "invalid interprocessor interrupt number: %d", regs.int_no);
	}
#endif
	set_int(0);
	set_cpu_interrupt_flag(1); /* assembly code will issue sti */
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void entry_syscall_handler(volatile registers_t regs)
{
	add_atomic(&int_count[0x80], 1);
	set_int(0);
	if(regs.eax == 128) {
		/* the injection code at the end of the signal handler calls
		 * a syscall with eax = 128. So here we handle returning from
		 * a signal handler. First, copy back the old registers, and
		 * reset flags and signal stuff */
		memcpy((void *)&regs, (void *)&current_task->reg_b, sizeof(registers_t));
		current_task->sig_mask = current_task->old_mask;
		current_task->cursig=0;
		current_task->flags &= ~TF_INSIG;
		current_task->flags &= ~TF_JUMPIN;
		set_cpu_interrupt_flag(1);
#if CONFIG_SMP
		lapic_eoi();
#endif
		return;
	}
	assert(!current_task->sysregs && !current_task->regs);
	current_task->regs = &regs;
	current_task->sysregs = &regs;
	syscall_handler(&regs);
	set_int(0);
	current_task->sysregs=0;
	current_task->regs=0;
	set_cpu_interrupt_flag(1);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* This gets called from our ASM interrupt handler stub. */
void isr_handler(volatile registers_t regs)
{
	set_int(0);
	add_atomic(&int_count[regs.int_no], 1);
	char called=0;
	int i;
	for(i=0;i<256;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0] || interrupt_handlers[regs.int_no][i][1])
		{
			called = 1;
			if(interrupt_handlers[regs.int_no][i][0])
				(interrupt_handlers[regs.int_no][i][0])(regs);
		}
	}
	
	set_int(0);
	if(!called)
		faulted(regs.int_no);
	set_cpu_interrupt_flag(1);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void irq_handler(volatile registers_t regs)
{
	set_int(0);
	char clear_regs=0;
	if(current_task && !current_task->regs) {
		clear_regs=1;
		current_task->regs = &regs;
	}
	current_task->flags |= TF_IN_INT;
	add_atomic(&int_count[regs.int_no], 1);
	int i;
	for(i=0;i<256;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0])
			(interrupt_handlers[regs.int_no][i][0])(regs);
	}
	
	
	
	set_int(0);
	if(current_task && clear_regs)
		current_task->regs=0;
	set_cpu_interrupt_flag(1);
	current_task->flags &= ~TF_IN_INT;
	if(interrupt_controller == IOINT_PIC) ack_pic(regs.int_no);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void int_sys_init()
{
	for(int i=0;i<256;i++)
	{
		stage2_count[i] = 0;
		for(int j=0;j<256;j++)
		{
			interrupt_handlers[i][j][0] = interrupt_handlers[i][j][1] = 0;
		}
	}
	mutex_create(&isr_lock, 0);
#if CONFIG_MODULES
	add_kernel_symbol(register_interrupt_handler);
	add_kernel_symbol(unregister_interrupt_handler);
	_add_kernel_symbol((unsigned)interrupt_handlers, "interrupt_handlers");
#endif
}

void print_stack_trace(unsigned int max)
{
	volatile unsigned int *ebp = (volatile unsigned *)&(max);
	ebp -= 2;
	kprintf("Stack Trace:\n");
	unsigned int frame;
	for(frame = 0; frame < max && ebp; ++frame)
	{
		unsigned int eip = ebp[1];
		if(eip == 0)
			break;
		const char *g = elf_lookup_symbol (eip, &kernel_elf);
		printk (5, "   [0x%x] %s\n", eip, g ? g : "(unknown)");
		ebp = (volatile unsigned int *)(ebp[0]);
	}
}

int proc_read_int(char *buf, int off, int len)
{
	int i, total_len=0;/*
	total_len += proc_append_buffer(buf, "ISR \t\t| HANDLERS | BLOCKED HANDLERS |            COUNT\n", total_len, -1, off, len);
	for(i=0;i<256;i++)
	{
		handlist_t *f = &interrupt_handlers[i];
		if(int_count[i] || f->handler)
		{
			int num_h=0, num_w=0;
			while(f)
			{
				if(f->handler) {
					num_h++;
					if(!f->block)
						num_w++;
				}
				f=f->next;
			}
			char t[128];
			sprintf(t, "%3d %s\t| %8d | %16d | %16d\n", i, special_names(i), num_h, num_h-num_w, int_count[i]);
			total_len += proc_append_buffer(buf, t, total_len, -1, off, len);
		}
	}*/
	return total_len;
}

char *exception_messages[] =
{
 "Division By Zero",
 "Debug",
 "Non Maskable Interrupt",
 "Breakpoint",
 "Into Detected Overflow",
 "Out of Bounds",
 "Invalid Opcode",
 "No Coprocessor",

 "Double Fault",
 "Coprocessor Segment Overrun", //9
 "Bad TSS",
 "Segment Not Present",
 "Stack Fault",
 "General Protection Fault",
 "Page Fault",
 "Unknown Interrupt",

 "Coprocessor Fault",
 "Alignment Check?",
 "Machine Check!",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",

 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved",
 "Reserved"
};
