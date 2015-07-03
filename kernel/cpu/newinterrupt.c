#include <sea/cpu/interrupt.h>
#include <sea/cpu/registers.h>
#include <sea/mutex.h>
#include <sea/lib/timer.h>
#include <sea/kernel.h>
#include <sea/vsprintf.h>
#include <sea/tm/process.h>
#include <sea/fs/kerfs.h>
struct interrupt_handler {
	void (*fn)(int nr, int flags);
};

#define INTR_USER   1
#define INTR_INTR   2
#define INTR_KERNEL 4

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

static struct interrupt_handler interrupt_handlers[MAX_INTERRUPTS][MAX_HANDLERS];
static unsigned long interrupt_counts[256];
static struct timer interrupt_timers[256];
static mutex_t isr_lock, s2_lock;
char interrupt_controller=0;

int cpu_interrupt_register_handler(int num, void (*fn)(int, int))
{
	mutex_acquire(&isr_lock);
	int i;
	for(i=0;i<MAX_HANDLERS;i++)
	{
		if(!interrupt_handlers[num][i].fn)
		{
			interrupt_handlers[num][i].fn = fn;
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
	if(!interrupt_handlers[n][id].fn)
		panic(0, "tried to unregister an empty interrupt handler");
	interrupt_handlers[n][id].fn = 0;
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
	arch_cpu_print_reg_state(regs);
	panic(PANIC_NOSYNC | (fuckoff == 3 ? PANIC_VERBOSE : 0), exception_messages[fuckoff]);
}

static void faulted(int fuckoff, int userspace, addr_t ip, long err_code, registers_t *regs)
{
	/* TODO: do we need to check all of these? */
	if(!current_task || current_task->system || !userspace)
	{
		kernel_fault(fuckoff, ip, err_code, regs);
	} else
	{
		printk(5, "%s occured in task %d (ip=%x, err=%x (%d)): He's dead, Jim.\n", 
				exception_messages[fuckoff], current_thread->tid, ip, err_code, err_code);
		/* we die for different reasons on different interrupts */
		switch(fuckoff)
		{
			case 0: case 5: case 6: case 13:
				current_thread->sigd = SIGILL;
				break;
			case 1: case 3: case 4:
				current_thread->sigd = SIGTRAP;
				break;
			case 8: case 18:
				current_thread->sigd = SIGABRT;
				break;
			default:
				tm_kill_thread(current_thread);
				break;
		}
		/* the above signals WILL be handled, since at the end of tm_tm_schedule(), it checks
		 * for signals. Since we are returning to user-space here, the handler will always run */
		tm_schedule();
	}
}

void cpu_interrupt_syscall_entry(registers_t *regs, int syscall_num)
{
	cpu_interrupt_set(0);
	add_atomic(&interrupt_counts[0x80], 1);
	if(current_thread->flags & TF_IN_INT) /* TODO: Better way of determining this? */
		panic(0, "attempted to enter syscall while handling an interrupt");
	tm_raise_flag(TF_IN_INT);
	if(syscall_num == 128) {
		/* TODO: RETURN SIGNAL HANDLING */
	} else {
		current_thread->regs = current_thread->sysregs = regs;
		syscall_handler(regs);
		assert(!cpu_interrupt_get_flag());
	}

	if(current_thread->flags & CPU_NEED_RESCHED)
		tm_schedule();

	cpu_interrupt_set(0);
	current_thread->sysregs = current_thread->regs = 0;
	cpu_interrupt_set_flag(1);
	tm_lower_flag(TF_IN_INT);
}

void cpu_interrupt_isr_entry(registers_t *regs, int int_no, addr_t return_address)
{
	/* this is explained in the IRQ handler */
	int previous_interrupt_flag = cpu_interrupt_set(0);
	add_atomic(&interrupt_count[int_no], 1);
	/* check if we're interrupting kernel code, and set the interrupt
	 * handling flag */
	char already_in_interrupt = 0;
	if(current_thread->flags & TF_IN_INT)
		already_in_interrupt = 1;
	tm_raise_flag(TF_IN_INT);
	/* run the stage1 handlers, and see if we need any stage2s. And if we
	 * don't handle it at all, we need to actually fault to handle the error
	 * and kill the process or kernel panic */
	int started = timer_start(&s1_timers[int_no]);
	char called = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[int_no][i].fn)
		{
			interrupt_handlers_s1[int_no][i].fn(regs);
			called = 1;
		}
	}
	if(started) timer_stop(&s1_timers[int_no]);
	cpu_interrupt_set(0);
	/* if it went unhandled, kill the process or panic */
	if(!called)
		faulted(int_no, !already_in_interrupt, return_address, regs->err_code, regs);

	if(current_thread->flags & CPU_NEED_RESCHED)
		tm_schedule();
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
	if(current_thread && !current_thread->regs) {
		/* of course, if we are already inside an interrupt, we shouldn't
		 * overwrite those. Also, we remember if we've saved this set of registers
		 * for later use */
		clear_regs=1;
		current_thread->regs = regs;
	}
	/* check if we're interrupting kernel code */
	char already_in_interrupt = 0;
	if(current_thread->flags & TF_IN_INT)
		already_in_interrupt = 1;
	/* ...and set the flag so we know we're in an interrupt */
	tm_raise_flag(TF_IN_INT);
	/* now, run through the stage1 handlers, and see if we need any
	 * stage2 handlers to run later */
	int s1started = timer_start(&s1_timers[int_no]);
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers_s1[int_no][i].fn)
			(interrupt_handlers_s1[int_no][i].fn)(regs);
	}
	if(s1started) timer_stop(&s1_timers[int_no]);
	if(current_thread->flags & CPU_NEED_RESCHED)
		tm_schedule();
	assert(!cpu_interrupt_get_flag());
	/* ok, now lets clean up */
	cpu_interrupt_set(0);
	/* clear the registers if we saved the ones from this interrupt */
	if(current_thread && clear_regs)
		current_thread->regs=0;
	/* restore the flag in the cpu struct. The assembly routine will
	 * call iret, which will also restore the EFLAG state to what
	 * it was before, including the interrupts-enabled bit in eflags */
	cpu_interrupt_set_flag(previous_interrupt_flag);
	/* and clear the state flag if this is going to return to user-space code */
	if(!already_in_interrupt)
		tm_lower_flag(TF_IN_INT);
}

void interrupt_init()
{
	for(int i=0;i<MAX_INTERRUPTS;i++)
	{
		timer_create(&interrupt_timers[i], 0);
		for(int j=0;j<MAX_HANDLERS;j++)
		{
			interrupt_handlers[i][j].fn = 0;
		}
	}
	
	mutex_create(&isr_lock, 0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(interrupt_register_handler);
	loader_add_kernel_symbol(interrupt_unregister_handler);
	loader_do_add_kernel_symbol((addr_t)interrupt_handlers, "interrupt_handlers_s1");
	loader_add_kernel_symbol(cpu_interrupt_set);
	loader_add_kernel_symbol(cpu_interrupt_set_flag);
#endif
}

int cpu_interrupt_set(unsigned _new)
{
	/* need to make sure we don't get interrupted... */
	arch_interrupt_disable();
	struct cpu *current_cpu = cpu_get_current();
	unsigned old = current_cpu->flags & CPU_INTER;
	if(!_new) {
		arch_interrupt_disable();
		current_cpu->flags &= ~CPU_INTER;
	} else {
		arch_interrupt_enable();
		current_cpu->flags |= CPU_INTER;
	}
	cpu_put_current(current_cpu);
	return old;
}

void cpu_interrupt_set_flag(int flag)
{
	struct cpu *current_cpu = cpu_get_current();
	if(flag)
		current_cpu->flags |= CPU_INTER; /* TODO: make these atomic */
	else
		current_cpu->flags &= ~CPU_INTER;
	cpu_put_current(cpu);
}

int cpu_interrupt_get_flag()
{
	struct cpu *current_cpu = cpu_get_current();
	int flag = current_cpu->flags&CPU_INTER;
	cpu_put_current(cpu);
}

int kerfs_int_report(size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"INT: # CALLS\tMIN\t      MAX\t    MEAN\n");
	for(int i=0;i<256;i++) {
		if(!int_count[i])
			continue;
		KERFS_PRINTF(offset, length, buf, current,
				"%3d: %-5d\n   "
				"| 1 -> %9d\t%9d\t%9d\n   ",
				i, int_count[i], 
				interrupt_timers[i].runs > 0 ?
					(uint32_t)interrupt_timers[i].min / 1000 : 
					0,
				(uint32_t)interrupt_timers[i].max / 1000,
				(uint32_t)interrupt_timers[i].mean / 1000);
	}
	return current;
}

