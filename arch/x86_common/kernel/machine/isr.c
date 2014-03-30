/* Privides functions for interrupt handling. */
#include <sea/kernel.h>
#include <sea/cpu/interrupt.h>
#include <asm/system.h>
#include <sea/tm/process.h>
#include <sea/cpu/processor.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>
#include <sea/fs/proc.h>
#include <sea/tm/schedule.h>
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

/* 3D array. So, the first "dimension" is all 255 of the possible
 * x86 processor interrupts. The second is all 255 of the allowed
 * handlers per interrupt. The third is the two stages of handlers
 * per interrupt handler. See below */
isr_t interrupt_handlers[MAX_INTERRUPTS][MAX_HANDLERS][2];
unsigned int stage2_count[256];
volatile long int_count[256];
mutex_t isr_lock, s2_lock;
char interrupt_controller=0;
/* if this is set to true, there may be a stage2 handler waiting to
 * be run. This is not always true though, if for instance another
 * tasks handles the stage2s first. */
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
 *         tm_delay until a task is able to run this handler. If a userspace
 *         task handles the interrupt, it will probably run the second
 *         stage handler right away.
 */

int arch_interrupt_register_handler(u8int num, isr_t stage1_handler, isr_t stage2_handler)
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

void arch_interrupt_unregister_handler(u8int n, int id)
{
	mutex_acquire(&isr_lock);
	if(!interrupt_handlers[n][id][0] && !interrupt_handlers[n][id][1])
		panic(0, "tried to unregister an empty interrupt handler");
	interrupt_handlers[n][id][0] = interrupt_handlers[n][id][1] = 0;
	mutex_release(&isr_lock);
}

static void kernel_fault(int fuckoff, addr_t ip)
{
	kprintf("Kernel Exception #%d: ", fuckoff);
	if(kernel_task)
		printk(5, "Occured in task %d during systemcall %d (F=%d).\n",
			current_task->pid, current_task->system, current_task->flag);
	kprintf("return IP = %x\n", ip);
	panic(PANIC_NOSYNC | (fuckoff == 3 ? PANIC_VERBOSE : 0), exception_messages[fuckoff]);
}

static const char *special_names(int i)
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

static void faulted(int fuckoff, int userspace, addr_t ip)
{
	if(current_task == kernel_task || !current_task || current_task->system || !userspace)
	{
		kernel_fault(fuckoff, ip);
	} else
	{
		printk(5, "%s occured in task %d (F=%d, ip=%x): He's dead, Jim.\n", 
				exception_messages[fuckoff], current_task->pid, current_task->flag, ip);
		/* we die for different reasons on different interrupts */
		switch(fuckoff)
		{
			case 0: case 5: case 6: case 13:
				current_task->sigd = SIGILL;
				break;
			case 1: case 3: case 4:
				current_task->sigd = SIGTRAP;
				break;
			case 8: case 18:
				current_task->sigd = SIGABRT;
				break;
			default:
				tm_kill_process(current_task->pid);
				break;
		}
		/* the above signals WILL be handled, since at the end of tm_tm_schedule(), it checks
		 * for signals. Since we are returning to user-space here, the handler will always run */
		while(!tm_schedule());
	}
}

/* don't need to worry about other processors getting in the way here, since
 * this is only used if SMP is disabled or unavailable */
static void ack_pic(int n)
{
	assert(interrupt_controller == IOINT_PIC);
	if(n >= IRQ0 && n < IRQ15) {
		if (n >= 40)
			outb(0xA0, 0x20);
		outb(0x20, 0x20);
	}
}

void arch_interrupt_ipi_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	int previous_interrupt_flag = interrupt_set(0);
	add_atomic(&int_count[regs.int_no], 1);
#if CONFIG_SMP
	/* delegate to the proper handler, in ipi.c */
	switch(regs.int_no) {
		case IPI_DEBUG:
		case IPI_SHUTDOWN:
		case IPI_PANIC:
			x86_cpu_handle_ipi_cpu_halt(regs);
			break;
		case IPI_SCHED:
			x86_cpu_handle_ipi_reschedule(regs);
			break;
		case IPI_TLB:
			x86_cpu_handle_ipi_tlb(regs);
			break;
		case IPI_TLB_ACK:
			x86_cpu_handle_ipi_tlb_ack(regs);
			break;
		default:
			panic(PANIC_NOSYNC, "invalid interprocessor interrupt number: %d", regs.int_no);
	}
#endif
	assert(!interrupt_set(0));
	interrupt_set_flag(previous_interrupt_flag); /* assembly code will issue sti */
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* this should NEVER enter from an interrupt handler, 
 * and only from kernel code in the one case of calling
 * sys_setup() */
void arch_interrupt_syscall_handler(volatile registers_t regs)
{
	/* don't need to save the flag here, since it will always be true */
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(regs.int_no == 0x80 && ((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	interrupt_set(0);
	add_atomic(&int_count[0x80], 1);
	if(current_task->flags & TF_IN_INT)
		panic(0, "attempted to enter syscall while handling an interrupt");
	/* set the interrupt handling flag... */
	tm_raise_flag(TF_IN_INT);
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	if(regs.rax == 128) {
#elif CONFIG_ARCH == TYPE_ARCH_X86
	if(regs.eax == 128) {
#endif
		/* the injection code at the end of the signal handler calls
		 * a syscall with eax = 128. So here we handle returning from
		 * a signal handler. First, copy back the old registers, and
		 * reset flags and signal stuff */
		memcpy((void *)&regs, (void *)&current_task->reg_b, sizeof(registers_t));
		current_task->sig_mask = current_task->old_mask;
		current_task->cursig=0;
		tm_lower_flag(TF_INSIG);
		tm_lower_flag(TF_JUMPIN);
	} else {
		assert(!current_task->sysregs && !current_task->regs);
		/* otherwise, this is a normal system call. Save the regs for modification
		 * for signals and exec */
		current_task->regs = &regs;
		current_task->sysregs = &regs;
		syscall_handler(&regs);
		assert(!interrupt_get_flag());
		/* handle stage2's here...*/
		if(maybe_handle_stage_2) {
			maybe_handle_stage_2 = 0;
			mutex_acquire(&s2_lock);
			for(int i=0;i<MAX_INTERRUPTS;i++)
			{
				if(stage2_count[i])
				{
					sub_atomic(&stage2_count[i], 1);
					for(int j=0;j<MAX_HANDLERS;j++) {
						if(interrupt_handlers[i][j][1]) {
							(interrupt_handlers[i][j][1])(&regs);
						}
					}
				}
			}
			mutex_release(&s2_lock);
		}
		assert(!interrupt_get_flag());
	}
	assert(!interrupt_set(0));
	current_task->sysregs=0;
	current_task->regs=0;
	/* we don't need worry about this being wrong, since we'll always be returning to
	 * user-space code */
	interrupt_set_flag(1);
	/* we're never returning to an interrupt, so we can
	 * safely reset this flag */
	tm_lower_flag(TF_IN_INT);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* This gets called from our ASM interrupt handler stub. */
void arch_interrupt_isr_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20));
#endif
	/* this is explained in the IRQ handler */
	int previous_interrupt_flag = interrupt_set(0);
	add_atomic(&int_count[regs.int_no], 1);
	/* check if we're interrupting kernel code, and set the interrupt
	 * handling flag */
	char already_in_interrupt = 0;
	if(current_task->flags & TF_IN_INT)
		already_in_interrupt = 1;
	tm_raise_flag(TF_IN_INT);
	/* run the stage1 handlers, and see if we need any stage2s. And if we
	 * don't handle it at all, we need to actually fault to handle the error
	 * and kill the process or kernel panic */
	char called=0;
	char need_second_stage = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0] || interrupt_handlers[regs.int_no][i][1])
		{
			/* we're able to handle the error! */
			called = 1;
			if(interrupt_handlers[regs.int_no][i][0])
				(interrupt_handlers[regs.int_no][i][0])(&regs);
			if(interrupt_handlers[regs.int_no][i][1])
				need_second_stage = 1;
		}
	}
	if(need_second_stage) {
		/* we need to run a second stage handler. Indicate that here... */
		add_atomic(&stage2_count[regs.int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	/* clean up... Also, we don't handle stage 2 in ISR handling, since this
	 can occur from within a stage2 handler */
	assert(!interrupt_set(0));
	/* if it went unhandled, kill the process or panic */
	if(!called)
		faulted(regs.int_no, !already_in_interrupt, regs.eip);
	/* restore previous interrupt state */
	interrupt_set_flag(previous_interrupt_flag);
	if(!already_in_interrupt)
		tm_lower_flag(TF_IN_INT);
	/* send out the EOI... */
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void arch_interrupt_irq_handler(volatile registers_t regs)
{
#if CONFIG_ARCH == TYPE_ARCH_X86_64
	assert(((regs.ds&(~0x7)) == 0x10 || (regs.ds&(~0x7)) == 0x20) && ((regs.cs&(~0x7)) == 0x8 || (regs.cs&(~0x7)) == 0x18));
#endif
	/* ok, so the assembly entry function clears interrupts in the cpu, 
	 * but the kernel doesn't know that yet. So we clear the interrupt
	 * flag in the cpu structure as part of the normal interrupt_set call, but
	 * it returns the interrupts-enabled flag from BEFORE the interrupt
	 * was recieved! Fuckin' brilliant! Back up that flag, so we can
	 * properly restore the flag later. */
	int previous_interrupt_flag = interrupt_set(0);
	add_atomic(&int_count[regs.int_no], 1);
	/* save the registers so we can screw with iret later if we need to */
	char clear_regs=0;
	if(current_task && !current_task->regs) {
		/* of course, if we are already inside an interrupt, we shouldn't
		 * overwrite those. Also, we remember if we've saved this set of registers
		 * for later use */
		clear_regs=1;
		current_task->regs = &regs;
	}
	/* check if we're interrupting kernel code */
	char already_in_interrupt = 0;
	if(current_task->flags & TF_IN_INT)
		already_in_interrupt = 1;
	/* ...and set the flag so we know we're in an interrupt */
	tm_raise_flag(TF_IN_INT);
	/* now, run through the stage1 handlers, and see if we need any
	 * stage2 handlers to run later */
	char need_second_stage = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[regs.int_no][i][0])
			(interrupt_handlers[regs.int_no][i][0])(&regs);
		if(interrupt_handlers[regs.int_no][i][1]) 
			need_second_stage = 1;
	}
	/* if we need a second stage handler, increment the count for this 
	 * interrupt number, and indicate that handlers should check for
	 * second stage handlers. */
	if(need_second_stage) {
		add_atomic(&stage2_count[regs.int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	assert(!interrupt_get_flag());
	/* ok, now are we allowed to handle stage2's right here? */
	if(!already_in_interrupt && (maybe_handle_stage_2||need_second_stage))
	{
		maybe_handle_stage_2 = 0;
		/* handle the stage2 handlers. NOTE: this may change to only 
		 * handling one interrupt, and/or one function. For now, this works. */
		mutex_acquire(&s2_lock);
		for(int i=0;i<MAX_INTERRUPTS;i++)
		{
			if(stage2_count[i])
			{
				/* decrease the count for this interrupt number, and loop through
				 * all the second stage handlers and run them */
				sub_atomic(&stage2_count[i], 1);
				for(int j=0;j<MAX_HANDLERS;j++) {
					if(interrupt_handlers[i][j][1]) {
						(interrupt_handlers[i][j][1])(&regs);
					}
				}
			}
		}
		mutex_release(&s2_lock);
		assert(!interrupt_get_flag());
	}
	/* ok, now lets clean up */
	assert(!interrupt_set(0));
	/* clear the registers if we saved the ones from this interrupt */
	if(current_task && clear_regs)
		current_task->regs=0;
	/* restore the flag in the cpu struct. The assembly routine will
	 * call iret, which will also restore the EFLAG state to what
	 * it was before, including the interrupts-enabled bit in eflags */
	interrupt_set_flag(previous_interrupt_flag);
	/* and clear the state flag if this is going to return to user-space code */
	if(!already_in_interrupt)
		tm_lower_flag(TF_IN_INT);
	/* and send out the EOIs */
	if(interrupt_controller == IOINT_PIC) ack_pic(regs.int_no);
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void arch_interrupt_reset_timer_state()
{
	if(interrupt_controller == IOINT_PIC) ack_pic(32);
}

/* make sure it eventually gets handled */
void __KT_try_handle_stage2_interrupts()
{
	if(maybe_handle_stage_2)
	{
		int old = interrupt_set(0);
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
						(interrupt_handlers[i][j][1])(current_task->regs);
					}
				}
			}
		}
		mutex_release(&s2_lock);
		interrupt_set(old);
	}
}

void arch_interrupt_init()
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
	mutex_create(&s2_lock, 0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(arch_interrupt_register_handler);
	loader_add_kernel_symbol(arch_interrupt_unregister_handler);
	loader_do_add_kernel_symbol((addr_t)interrupt_handlers, "interrupt_handlers");
#endif
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

