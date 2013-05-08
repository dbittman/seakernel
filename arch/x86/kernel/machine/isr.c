/* Privides functions for interrupt handling. */
#include <kernel.h>
#include <isr.h>
#include <asm/system.h>
#include <task.h>
#include <cpu.h>
#include <elf.h>
#include <atomic.h>

extern char *exception_messages[];
isr_t interrupt_handlers[MAX_INTERRUPTS][MAX_HANDLERS][2];
unsigned int stage2_count[256];
volatile long int_count[256];
mutex_t isr_lock, s2_lock;
char interrupt_controller=0;
volatile char maybe_handle_stage_2=0;

/* interrupt handlers come in two types:
 * stage1: executed immediately when the interrupt is handled.
 *         this is less safe than a stage2 handler, and must be used
 *         carefully. No locks or interrupt state changes may be made
 *         inside this handler, because it can interrupt critical sections
 *         of kernel code. These should return ASAP.
 * stage2: executed whenever a task is at a safe point in execution to
 *         handle it. These may be written like normal kernel code,
 *         utilizing normal kernel features and functions. However, this
 *         code will not always run right away. There may be a small
 *         delay until a task is able to run this handler. If a userspace
 *         task handles the interrupt, it will probably run the second
 *         stage handler right away.
 */

int register_interrupt_handler(u8int num, isr_t stage1_handler, isr_t stage2_handler)
{
	mutex_acquire(&isr_lock);
	int i;
	for(i=0;i<MAX_HANDLERS;i++)
	{
		if(!interrupt_handlers[num][i][0] && !interrupt_handlers[num][i][1])
		{
			interrupt_handlers[num][i][0] = stage1_handler;
			interrupt_handlers[num][i][1] = stage2_handler;
			break;
		}
	}
	mutex_release(&isr_lock);
	if(i == MAX_HANDLERS) panic(0, "ran out of interrupt handlers");
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
	set_int(0);
	add_atomic(&int_count[0x80], 1);
	if(current_task->flags & TF_IN_INT)
		panic(0, "attempted to enter syscall while handling an interrupt");
	/* set the interrupt handling flag... */
	current_task->flags |= TF_IN_INT;
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
	} else {
		assert(!current_task->sysregs && !current_task->regs);
		current_task->regs = &regs;
		current_task->sysregs = &regs;
		syscall_handler(&regs);
		/* handle stage2's here...*/
		if(maybe_handle_stage_2) {
			mutex_acquire(&s2_lock);
			for(int i=0;i<MAX_INTERRUPTS;i++)
			{
				if(stage2_count[i])
				{
					sub_atomic(&stage2_count[i], 1);
					for(int j=0;j<MAX_HANDLERS;j++) {
						if(interrupt_handlers[i][j][1]) {
							(interrupt_handlers[i][j][1])(regs);
						}
					}
				}
			}
			mutex_release(&s2_lock);
		} 
	}
	set_int(0);
	current_task->sysregs=0;
	current_task->regs=0;
	set_cpu_interrupt_flag(1);
	current_task->flags &= ~TF_IN_INT;
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* This gets called from our ASM interrupt handler stub. */
void isr_handler(volatile registers_t regs)
{
	set_int(0);
	add_atomic(&int_count[regs.int_no], 1);
	/* check if we're interrupting kernel code, and set the interrupt
	 * handling flag */
	char already_in_interrupt = 0;
	if(current_task->flags & TF_IN_INT)
		already_in_interrupt = 1;
	current_task->flags |= TF_IN_INT;
	/* run the stage1 handlers, and see if we need any stage2s */
	char called=0;
	char need_second_stage = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0] || interrupt_handlers[regs.int_no][i][1])
		{
			called = 1;
			if(interrupt_handlers[regs.int_no][i][0])
				(interrupt_handlers[regs.int_no][i][0])(regs);
			if(interrupt_handlers[regs.int_no][i][1])
				need_second_stage = 1;
		}
	}
	if(need_second_stage) {
		add_atomic(&stage2_count[regs.int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	/* clean up... */
	set_int(0);
	if(!called)
		faulted(regs.int_no);
	set_cpu_interrupt_flag(1);
	current_task->flags &= ~TF_IN_INT;
	/* send out the EOI... */
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void irq_handler(volatile registers_t regs)
{
	set_int(0);
	add_atomic(&int_count[regs.int_no], 1);
	/* save the registers so we can screw with iret later if we need to */
	char clear_regs=0;
	if(current_task && !current_task->regs) {
		clear_regs=1;
		current_task->regs = &regs;
	}
	/* check if we're interrupting kernel code */
	char already_in_interrupt = 0;
	if(current_task->flags & TF_IN_INT)
		already_in_interrupt = 1;
	/* ...and set the flag so we know we're in an interrupt */
	current_task->flags |= TF_IN_INT;
	
	/* now, run through the stage1 handlers, and see if we need any
	 * stage2 handlers to run later */
	char need_second_stage = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0])
			(interrupt_handlers[regs.int_no][i][0])(regs);
		if(interrupt_handlers[regs.int_no][i][1]) need_second_stage = 1;
	}
	if(need_second_stage) {
		add_atomic(&stage2_count[regs.int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	
	/* ok, now are we allowed to handle stage2's right here? */
	if(!already_in_interrupt && maybe_handle_stage_2)
	{
		maybe_handle_stage_2 = 0;
		/* handle the stage2 handlers. NOTE: this may change to only 
		 * handling one interrupt, one function. For now, this works. */
		mutex_acquire(&s2_lock);
		for(int i=0;i<MAX_INTERRUPTS;i++)
		{
			if(stage2_count[i])
			{
				sub_atomic(&stage2_count[i], 1);
				for(int j=0;j<MAX_HANDLERS;j++) {
					if(interrupt_handlers[i][j][1]) {
						(interrupt_handlers[i][j][1])(regs);
					}
				}
			}
		}
		mutex_release(&s2_lock);
	}
	/* ok, now lets clean up */
	set_int(0);
	if(current_task && clear_regs)
		current_task->regs=0;
	set_cpu_interrupt_flag(1);
	current_task->flags &= ~TF_IN_INT;
	/* and send out the EOIs */
	if(interrupt_controller == IOINT_PIC) ack_pic(regs.int_no);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* make sure it eventually gets handled */
void __KT_try_handle_stage2_interrupts()
{
	if(maybe_handle_stage_2)
	{
		int old = set_int(0);
		maybe_handle_stage_2 = 0;
		/* handle the stage2 handlers. NOTE: this may change to only 
		 * handling one interrupt, one function. For now, this works. */
		mutex_acquire(&s2_lock);
		for(int i=0;i<MAX_INTERRUPTS;i++)
		{
			if(stage2_count[i])
			{
				sub_atomic(&stage2_count[i], 1);
				for(int j=0;j<MAX_HANDLERS;j++) {
					if(interrupt_handlers[i][j][1]) {
						(interrupt_handlers[i][j][1])(*current_task->regs);
					}
				}
			}
		}
		mutex_release(&s2_lock);
		set_int(old);
	}
}

void int_sys_init()
{
	for(int i=0;i<MAX_INTERRUPTS;i++)
	{
		stage2_count[i] = 0;
		for(int j=0;j<MAX_HANDLERS;j++)
		{
			interrupt_handlers[i][j][0] = interrupt_handlers[i][j][1] = 0;
		}
	}
	maybe_handle_stage_2 = 0;
	mutex_create(&isr_lock, 0);
#warning "MT_NOSCHED?"
	mutex_create(&s2_lock, 0);
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
	int i, total_len=0;
	total_len += proc_append_buffer(buf, "ISR \t\t|            COUNT\n", total_len, -1, off, len);
	for(i=0;i<MAX_INTERRUPTS;i++)
	{
		if(int_count[i])
		{
			char t[128];
			sprintf(t, "%3d %s\t| %16d\n", i, special_names(i), int_count[i]);
			total_len += proc_append_buffer(buf, t, total_len, -1, off, len);
		}
	}
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
