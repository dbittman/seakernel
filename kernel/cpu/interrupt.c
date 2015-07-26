#include <sea/cpu/interrupt.h>
#include <sea/cpu/registers.h>
#include <sea/mutex.h>
#include <sea/lib/timer.h>
#include <sea/kernel.h>
#include <sea/vsprintf.h>
#include <sea/tm/process.h>
#include <sea/fs/kerfs.h>
#include <sea/cpu/atomic.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/processor.h>
#include <sea/tm/workqueue.h>
#include <sea/tm/async_call.h>
#include <sea/debugger.h>
struct interrupt_handler {
	void (*fn)(registers_t *, int nr, int flags);
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
unsigned long interrupt_counts[256];
static struct timer interrupt_timers[256];
static mutex_t isr_lock, s2_lock;
char interrupt_controller=0;

int cpu_interrupt_register_handler(int num, void (*fn)(registers_t *, int, int))
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

void cpu_interrupt_unregister_handler(u8int n, int id)
{
	mutex_acquire(&isr_lock);
	if(!interrupt_handlers[n][id].fn)
		panic(0, "tried to unregister an empty interrupt handler");
	interrupt_handlers[n][id].fn = 0;
	mutex_release(&isr_lock);
}

void cpu_interrupt_schedule_stage2(struct async_call *call)
{
	struct cpu *c = cpu_get_current();
	workqueue_insert(&c->work, call);
	cpu_put_current(c);
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
	kprintf("Kernel Exception #%d: %s\n", fuckoff, exception_messages[fuckoff]);
	arch_cpu_print_reg_state(regs);
	panic(PANIC_INSTANT | PANIC_NOSYNC | (fuckoff == 3 ? PANIC_VERBOSE : 0), exception_messages[fuckoff]);
}

static void faulted(int fuckoff, int userspace, addr_t ip, long err_code, registers_t *regs)
{
	if(!current_thread || current_thread->system || !userspace)
	{
		if(fuckoff == 3)
			debugger_enter();
		else
			kernel_fault(fuckoff, ip, err_code, regs);
	} else
	{
		printk(5, "%s occured in task %d (ip=%x, err=%x (%d), usersp=%x): He's dead, Jim.\n", 
				exception_messages[fuckoff], current_thread->tid, ip, err_code, err_code, regs->useresp);
		/* we die for different reasons on different interrupts */
		switch(fuckoff)
		{
			case 0: case 5: case 6: case 13:
				tm_signal_send_thread(current_thread, SIGILL);
				break;
			case 1: case 3: case 4:
				tm_signal_send_thread(current_thread, SIGTRAP);
				break;
			case 8: case 18:
				tm_signal_send_thread(current_thread, SIGABRT);
				break;
			default:
				tm_signal_send_thread(current_thread, SIGABRT);
				break;
		}
		tm_thread_exit(-9);
	}
}

static inline void __setup_signal_handler(registers_t *regs)
{
	if(!current_thread->signal || !(current_thread->flags & THREAD_SIGNALED))
		return;
	struct sigaction *sa = &current_process->signal_act[current_thread->signal];
	arch_tm_userspace_signal_initializer(regs, sa);
	tm_thread_lower_flag(current_thread, THREAD_SIGNALED);
	current_thread->signal = 0;
}

extern void syscall_handler(registers_t *regs);
void cpu_interrupt_syscall_entry(registers_t *regs, int syscall_num)
{
	cpu_interrupt_set(0);
	assert(!current_thread->regs);
	current_thread->regs = regs;
	add_atomic(&interrupt_counts[0x80], 1);
	if(syscall_num == 128) {
		arch_tm_userspace_signal_cleanup(regs);
	} else {
		current_thread->regs = regs;
		syscall_handler(regs);
	}
	cpu_interrupt_set(0);
	__setup_signal_handler(regs);
	current_thread->regs = 0;
}

void cpu_interrupt_isr_entry(registers_t *regs, int int_no, addr_t return_address)
{
	int already_in_kernel = 0;
	cpu_interrupt_set(0);
	add_atomic(&interrupt_counts[int_no], 1);
	if(!current_thread->regs)
		current_thread->regs = regs;
	else
		already_in_kernel = 1;
	assert(add_atomic(&current_thread->interrupt_level, 1) > 0);
	int started = timer_start(&interrupt_timers[int_no]);
	char called = 0;
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[int_no][i].fn)
		{
			interrupt_handlers[int_no][i].fn(regs, int_no, 0);
			called = 1;
		}
	}
	if(started) timer_stop(&interrupt_timers[int_no]);
	cpu_interrupt_set(0);
	/* if it went unhandled, kill the process or panic */
	if(!called)
		faulted(int_no, !already_in_kernel, return_address, regs->err_code, regs);
	assert(sub_atomic(&current_thread->interrupt_level, 1) >= 0);
	if(!already_in_kernel) {
		__setup_signal_handler(regs);
		current_thread->regs = 0;
	}
}

void cpu_interrupt_irq_entry(registers_t *regs, int int_no)
{
	cpu_interrupt_set(0);
	add_atomic(&interrupt_counts[int_no], 1);
	int already_in_kernel = 0;
	if(!current_thread->regs)
		current_thread->regs = regs;
	else
		already_in_kernel = 1;
	assert(add_atomic(&current_thread->interrupt_level, 1) > 0);
	/* now, run through the stage1 handlers, and see if we need any
	 * stage2 handlers to run later */
	int s1started = timer_start(&interrupt_timers[int_no]);
	for(int i=0;i<MAX_HANDLERS;i++)
	{
		if(interrupt_handlers[int_no][i].fn)
			(interrupt_handlers[int_no][i].fn)(regs, int_no, 0);
	}
	if(s1started) timer_stop(&interrupt_timers[int_no]);
	cpu_interrupt_set(0);
	assert(sub_atomic(&current_thread->interrupt_level, 1) >= 0);
	if(!already_in_kernel) {
		__setup_signal_handler(regs);
		current_thread->regs = 0;
	}
}

/* this gets run when the interrupt hasn't returned yet, but it's now maybe 'safe' to
 * do things - specifically, we've been "ack'd", and interrupt_level is now
 * decremented */
void cpu_interrupt_post_handling(void)
{
	if(!current_thread->interrupt_level) {
		if(current_thread->flags & THREAD_EXIT)
			tm_thread_do_exit();
		if(current_thread->flags & THREAD_SCHEDULE)
			tm_schedule();
	}
}

void interrupt_init(void)
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
	loader_add_kernel_symbol(cpu_interrupt_register_handler);
	loader_add_kernel_symbol(cpu_interrupt_unregister_handler);
	loader_add_kernel_symbol(cpu_interrupt_schedule_stage2);
	loader_do_add_kernel_symbol((addr_t)interrupt_handlers, "interrupt_handlers");
	loader_add_kernel_symbol(cpu_interrupt_set);
#endif
}

int cpu_interrupt_set(unsigned _new)
{
	int old = cpu_interrupt_get_flag();
	if(!_new) {
		arch_interrupt_disable();
	} else {
		arch_interrupt_enable();
	}
	return old;
}

int cpu_interrupt_get_flag(void)
{
	return arch_cpu_get_interrupt_flag();
}

int kerfs_int_report(size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"INT: # CALLS\tMIN\t      MAX\t    MEAN\n");
	for(int i=0;i<256;i++) {
		if(!interrupt_counts[i])
			continue;
		KERFS_PRINTF(offset, length, buf, current,
				"%3d: %-5d\n   "
				"| 1 -> %9d\t%9d\t%9d\n   ",
				i, interrupt_counts[i], 
				interrupt_timers[i].runs > 0 ?
					(uint32_t)interrupt_timers[i].min / 1000 : 
					0,
				(uint32_t)interrupt_timers[i].max / 1000,
				(uint32_t)interrupt_timers[i].mean / 1000);
	}
	return current;
}

