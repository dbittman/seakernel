/* interrupt.c - architecture independent interrupt handling code.
 * 
 * These are a little different from normal arch-dependent interface functions, 
 * since they are backwards: The arch-dependent code calls these, not the other
 * way around. All functions in this file with names ending in _entry are entry
 * points into the architecture independent kernel, from the arch-dependent
 * interrupt handling code.
 */

#include <sea/cpu/interrupt.h>
#include <sea/cpu/registers.h>
#include <sea/cpu/processor.h>
#include <sea/tm/process.h>
#include <sea/kernel.h>
#include <sea/cpu/atomic.h>
#include <sea/tm/schedule.h>
#include <sea/loader/symbol.h>
#include <sea/fs/proc.h>
#include <sea/vsprintf.h>
#include <sea/syscall.h>
#include <sea/lib/timer.h>

/* okay, these aren't architecture independent exactly, but they're fine for now */
static char *exception_messages[] =
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

/* 2D array. So, the first "dimension" is all 255 of the possible
 * x86 processor interrupts. The second is all 255 of the allowed
 * handlers per interrupt.
 */
static isr_s1_handler_t interrupt_handlers_s1[MAX_INTERRUPTS][MAX_HANDLERS];
static isr_s2_handler_t interrupt_handlers_s2[MAX_INTERRUPTS][MAX_HANDLERS];
static struct timer s1_timers[256];
static struct timer s2_timers[256];
static unsigned int stage2_count[256];
volatile long int_count[256];
static mutex_t isr_lock, s2_lock;
char interrupt_controller=0;

/* if this is set to true, there may be a stage2 handler waiting to
 * be run. This is not always true though, if for instance another
 * tasks handles the stage2s first. */
static volatile char maybe_handle_stage_2=0;

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
int interrupt_register_handler(u8int num, isr_s1_handler_t stage1_handler, isr_s2_handler_t stage2_handler)
{
	mutex_acquire(&isr_lock);
	int i;
	for(i=0;i<MAX_HANDLERS;i++)
	{
		if(!interrupt_handlers_s1[num][i] && !interrupt_handlers_s2[num][i])
		{
			interrupt_handlers_s1[num][i] = stage1_handler;
			interrupt_handlers_s2[num][i] = stage2_handler;
			break;
		}
	}
	mutex_release(&isr_lock);
	if(i == MAX_HANDLERS) panic(0, "ran out of interrupt handlers");
	return i;
}

void interrupt_unregister_handler(u8int n, int id)
{
	mutex_acquire(&isr_lock);
	if(!interrupt_handlers_s1[n][id] && !interrupt_handlers_s2[n][id])
		panic(0, "tried to unregister an empty interrupt handler");
	interrupt_handlers_s1[n][id] = 0;
	interrupt_handlers_s2[n][id] = 0;
	mutex_release(&isr_lock);
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

static void kernel_fault(int fuckoff, addr_t ip, long err_code, registers_t *regs)
{
	kprintf("Kernel Exception #%d: ", fuckoff);
	if(kernel_task)
		printk(5, "Occured in task %d during systemcall %d (F=%d).\n",
			current_task->pid, current_task->system, current_task->flag);
	kprintf("return IP = %x, errcode = %x (%d)\n", ip, err_code, err_code);
	arch_cpu_print_reg_state(regs);
	panic(PANIC_NOSYNC | (fuckoff == 3 ? PANIC_VERBOSE : 0), exception_messages[fuckoff]);
}

static void faulted(int fuckoff, int userspace, addr_t ip, long err_code, registers_t *regs)
{
	if(current_task == kernel_task || !current_task || current_task->system || !userspace)
	{
		kernel_fault(fuckoff, ip, err_code, regs);
	} else
	{
		printk(5, "%s occured in task %d (F=%d, ip=%x, err=%x (%d)): He's dead, Jim.\n", 
				exception_messages[fuckoff], current_task->pid, current_task->flag, ip, err_code, err_code);
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

void cpu_interrupt_syscall_entry(registers_t *regs, int syscall_num)
{
	cpu_interrupt_set(0);
	add_atomic(&int_count[0x80], 1);
	if(current_task->flags & TF_IN_INT)
		panic(0, "attempted to enter syscall while handling an interrupt");
	/* set the interrupt handling flag... */
	tm_raise_flag(TF_IN_INT);
	if(syscall_num == 128) {
		/* the injection code at the end of the signal handler calls
		 * a syscall with eax = 128. So here we handle returning from
		 * a signal handler. First, copy back the old registers, and
		 * reset flags and signal stuff */
		memcpy((void *)regs, (void *)&current_task->reg_b, sizeof(registers_t));
		current_task->sig_mask = current_task->old_mask;
		current_task->cursig=0;
		tm_lower_flag(TF_INSIG);
		tm_lower_flag(TF_JUMPIN);
	} else {
		assert(!current_task->sysregs && !current_task->regs);
		/* otherwise, this is a normal system call. Save the regs for modification
		 * for signals and exec */
		current_task->regs = regs;
		current_task->sysregs = regs;
		syscall_handler(regs);
		assert(!cpu_interrupt_get_flag());
		/* handle stage2's here...*/
		if(maybe_handle_stage_2) {
			maybe_handle_stage_2 = 0;
			mutex_acquire(&s2_lock);
			for(int i=0;i<MAX_INTERRUPTS;i++)
			{
				if(stage2_count[i])
				{
					int started = timer_start(&s2_timers[i]);
					int  a =sub_atomic(&stage2_count[i], 1);
					for(int j=0;j<MAX_HANDLERS;j++) {
						if(interrupt_handlers_s2[i][j]) {
							(interrupt_handlers_s2[i][j])(i);
						}
					}
					if(started) timer_stop(&s2_timers[i]);
				}
			}
			mutex_release(&s2_lock);
		}
		assert(!cpu_interrupt_get_flag());
	}
	cpu_interrupt_set(0);
	current_task->sysregs=0;
	current_task->regs=0;
	/* we don't need worry about this being wrong, since we'll always be returning to
	 * user-space code */
	cpu_interrupt_set_flag(1);
	/* we're never returning to an interrupt, so we can
	 * safely reset this flag */
	tm_lower_flag(TF_IN_INT);
}

void cpu_interrupt_isr_entry(registers_t *regs, int int_no, addr_t return_address)
{
	/* this is explained in the IRQ handler */
	int previous_interrupt_flag = cpu_interrupt_set(0);
	add_atomic(&int_count[int_no], 1);
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
	int started = timer_start(&s1_timers[int_no]);
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers_s1[int_no][i] || interrupt_handlers_s2[int_no][i])
		{
			/* we're able to handle the error! */
			called = 1;
			if(interrupt_handlers_s1[int_no][i])
				(interrupt_handlers_s1[int_no][i])(regs);
			if(interrupt_handlers_s2[int_no][i])
				need_second_stage = 1;
		}
	}
	if(started) timer_stop(&s1_timers[int_no]);
	if(need_second_stage) {
		/* we need to run a second stage handler. Indicate that here... */
		add_atomic(&stage2_count[int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	/* clean up... Also, we don't handle stage 2 in ISR handling, since this
	 can occur from within a stage2 handler */
	cpu_interrupt_set(0);
	/* if it went unhandled, kill the process or panic */
	if(!called)
		faulted(int_no, !already_in_interrupt, return_address, regs->err_code, regs);
	/* restore previous interrupt state */
	cpu_interrupt_set_flag(previous_interrupt_flag);
	if(!already_in_interrupt)
		tm_lower_flag(TF_IN_INT);
}

void cpu_interrupt_irq_entry(registers_t *regs, int int_no)
{
	/* ok, so the assembly entry function clears interrupts in the cpu, 
	 * but the kernel doesn't know that yet. So we clear the interrupt
	 * flag in the cpu structure as part of the normal cpu_interrupt_set call, but
	 * it returns the interrupts-enabled flag from BEFORE the interrupt
	 * was recieved! Fuckin' brilliant! Back up that flag, so we can
	 * properly restore the flag later. */
	int previous_interrupt_flag = cpu_interrupt_set(0);
	add_atomic(&int_count[int_no], 1);
	/* save the registers so we can screw with iret later if we need to */
	char clear_regs=0;
	if(current_task && !current_task->regs) {
		/* of course, if we are already inside an interrupt, we shouldn't
		 * overwrite those. Also, we remember if we've saved this set of registers
		 * for later use */
		clear_regs=1;
		current_task->regs = regs;
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
	int s1started = timer_start(&s1_timers[int_no]);
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers_s1[int_no][i])
			(interrupt_handlers_s1[int_no][i])(regs);
		if(interrupt_handlers_s2[int_no][i]) 
			need_second_stage = 1;
	}
	if(s1started) timer_stop(&s1_timers[int_no]);
	/* if we need a second stage handler, increment the count for this 
	 * interrupt number, and indicate that handlers should check for
	 * second stage handlers. */
	if(need_second_stage) {
		int r = add_atomic(&stage2_count[int_no], 1);
		maybe_handle_stage_2 = 1;
	}
	assert(!cpu_interrupt_get_flag());
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
				int started = timer_start(&s2_timers[i]);
				int a = sub_atomic(&stage2_count[i], 1);
				for(int j=0;j<MAX_HANDLERS;j++) {
					if(interrupt_handlers_s2[i][j]) {
						(interrupt_handlers_s2[i][j])(i);
					}
				}
				if(started) timer_stop(&s2_timers[i]);
			}
		}
		mutex_release(&s2_lock);
		assert(!cpu_interrupt_get_flag());
	}
	/* ok, now lets clean up */
	cpu_interrupt_set(0);
	/* clear the registers if we saved the ones from this interrupt */
	if(current_task && clear_regs)
		current_task->regs=0;
	/* restore the flag in the cpu struct. The assembly routine will
	 * call iret, which will also restore the EFLAG state to what
	 * it was before, including the interrupts-enabled bit in eflags */
	cpu_interrupt_set_flag(previous_interrupt_flag);
	/* and clear the state flag if this is going to return to user-space code */
	if(!already_in_interrupt)
		tm_lower_flag(TF_IN_INT);
}

/* make sure it eventually gets handled */
void __KT_try_handle_stage2_interrupts()
{
	/* TODO: don't run this if there aren't any pending interrupts */
	if(!mutex_is_locked(&s2_lock))
	{
		int old = cpu_interrupt_set(0);
		int a;
		maybe_handle_stage_2 = 0;
		/* handle the stage2 handlers. NOTE: this may change to only 
		 * handling one interrupt, one function. For now, this works. */
		mutex_acquire(&s2_lock);
		for(int i=0;i<MAX_INTERRUPTS;i++)
		{
			if(stage2_count[i])
			{
				int started = timer_start(&s2_timers[i]);
				a = sub_atomic(&stage2_count[i], 1);
				for(int j=0;j<MAX_HANDLERS;j++) {
					if(interrupt_handlers_s2[i][j]) {
						(interrupt_handlers_s2[i][j])(i);
					}
				}
				if(started) timer_stop(&s2_timers[i]);
			}
		}
		mutex_release(&s2_lock);
		cpu_interrupt_set(old);
	}
}

void interrupt_init()
{
	for(int i=0;i<MAX_INTERRUPTS;i++)
	{
		stage2_count[i] = 0;
		timer_create(&s1_timers[i], 0);
		timer_create(&s2_timers[i], 0);
		for(int j=0;j<MAX_HANDLERS;j++)
		{
			interrupt_handlers_s1[i][j] = 0;
			interrupt_handlers_s2[i][j] = 0;
		}
	}
	
	maybe_handle_stage_2 = 0;
	mutex_create(&isr_lock, 0);
	mutex_create(&s2_lock, 0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(interrupt_register_handler);
	loader_add_kernel_symbol(interrupt_unregister_handler);
	loader_do_add_kernel_symbol((addr_t)interrupt_handlers_s1, "interrupt_handlers_s1");
	loader_do_add_kernel_symbol((addr_t)interrupt_handlers_s2, "interrupt_handlers_s2");
	loader_add_kernel_symbol(cpu_interrupt_set);
	loader_add_kernel_symbol(cpu_interrupt_set_flag);
#endif
}

int cpu_interrupt_set(unsigned _new)
{
	/* need to make sure we don't get interrupted... */
	arch_interrupt_disable();
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	unsigned old = cpu ? cpu->flags&CPU_INTER : 0;
	if(!_new) {
		arch_interrupt_disable();
		if(cpu) cpu->flags &= ~CPU_INTER;
	} else if(!cpu || cpu->flags&CPU_RUNNING) {
		arch_interrupt_enable();
		if(cpu) cpu->flags |= CPU_INTER;
	}
	return old;
}

void cpu_interrupt_set_flag(int flag)
{
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	if(!cpu) return;
	if(flag)
		cpu->flags |= CPU_INTER;
	else
		cpu->flags &= ~CPU_INTER;
}

int cpu_interrupt_get_flag()
{
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	return (cpu ? (cpu->flags&CPU_INTER) : 0);
}

int kerfs_int_report(size_t offset, size_t length, char *buf)
{
	size_t dl = 0;
	char tmp[10000];
	dl = snprintf(tmp, 100, "INT: # CALLS\tMIN\t      MAX\t    MEAN\n");
	for(int i=0;i<256;i++) {
		if(!int_count[i])
			continue;
		char line[256];
		int r = snprintf(line, 256, "%3d: %-5d\n   "
				"| 1 -> %9d\t%9d\t%9d\n   "
				"| 2 -> %9d\t%9d\t%9d\n\n",
				i, int_count[i], 
				s1_timers[i].runs > 0 ?
					(uint32_t)s1_timers[i].min / 1000 : 
					0,
				(uint32_t)s1_timers[i].max / 1000,
				(uint32_t)s1_timers[i].mean / 1000,
				s2_timers[i].runs > 0 ? 
					(uint32_t)s2_timers[i].min / 1000 : 
					0,
				(uint32_t)s2_timers[i].max / 1000,
				(uint32_t)s2_timers[i].mean / 1000);
		assert(dl+r < 10000);
		memcpy(tmp + dl, line, r);
		dl += r;	
	}
	if(offset > dl)
		return 0;
	if(offset + length > dl)
		length = dl - offset;
	memcpy(buf, tmp + offset, length);
	return length;
}

