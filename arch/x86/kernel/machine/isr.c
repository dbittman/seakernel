/* Privides functions for interrupt handling. */
#include <kernel.h>
#include <isr.h>
#include <asm/system.h>
#include <task.h>
#include <cpu.h>
#include <elf.h>
#include <atomic.h>
#warning "This is not safe! For SMP or otherwise!"
extern char *exception_messages[];
handlist_t interrupt_handlers[256];
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
void register_interrupt_handler(u8int num, isr_t handler)
{
	handlist_t *n;
	cli();
	if((kernel_state_flags&KSF_MMU) && interrupt_handlers[num].handler)
	{
		handlist_t *f = &interrupt_handlers[num];
		handlist_t *h = f->next;
		n = (handlist_t *)kmalloc(sizeof(*f));
		f->next = n;
		n->next = h;
		if(h)
			h->prev = n;
		n->prev = f;
	}
	else
		n = &interrupt_handlers[num];
	n->handler = handler;
	n->n = num;
	sti();
}

void unregister_interrupt_handler(u8int n, isr_t handler)
{
	cli();
	handlist_t *f = &interrupt_handlers[n];
	while(f)
	{
		if(f->handler == handler)
		{
			if(f->prev)
				f->prev->next=f->next;
			if(f->next)
				f->next->prev=f->prev;
			f->handler=0;
			if(f->prev)
				kfree(f);
		}
		f=f->next;
	}
	sti();
}

handlist_t *get_interrupt_handler(u8int n)
{
	return &interrupt_handlers[n];
}

void kernel_fault(int fuckoff)
{
	kprintf("Kernel Exception #%d: ", fuckoff);
	printk(5, "Occured in task %d during systemcall %d (F=%d).\n",
			current_task->pid, current_task->system, current_task->flag);
	
	panic(0, exception_messages[fuckoff]);
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

void ack(int n)
{
	if(n >= IRQ0 && n < IRQ15) {
		if (n >= 40)
			outb(0xA0, 0x20);
		outb(0x20, 0x20);
	}
}

void entry_syscall_handler(volatile registers_t regs)
{
	add_atomic(&int_count[0x80], 1);
	ack(0x80);
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
		return;
	}
	/* we don't change the interrupt status flag in the CPU struct until
	 * we get here because the above code will simply return before it
	 * because it becomes a problem. */
	set_int(0);
	assert(!current_task->sysregs && !current_task->regs);
	current_task->regs = &regs;
	current_task->sysregs = &regs;
	syscall_handler(&regs);
	current_task->sysregs=0;
	current_task->regs=0;
#if CONFIG_SMP
	lapic_eoi();
#endif
}

/* This gets called from our ASM interrupt handler stub. */
void isr_handler(volatile registers_t regs)
{
	set_int(0);
	add_atomic(&int_count[regs.int_no], 1);
	ack(regs.int_no);
	//char x[12];
	//sprintf(x, "ISRhi: %d\n", regs.int_no);
	//serial_puts_nolock(0, x);
	if(regs.int_no == 100)
	{
#if CONFIG_SMP
		lapic_eoi();
#endif
		return;
	}
	char called=0;
	handlist_t *f = &interrupt_handlers[regs.int_no];
	while(f)
	{
		isr_t handler = f->handler;
		if(handler) {
			if(!f->block)
				handler(regs);
			called=1;
		}
		f=f->next;
	}
	if(!called)
		faulted(regs.int_no);
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
	add_atomic(&int_count[regs.int_no], 1);
	ack(regs.int_no);
	handlist_t *f = &interrupt_handlers[regs.int_no];
//	char x[8];
//	sprintf(x, "hi: %d\n", regs.int_no);
//	serial_puts_nolock(0, x);
	while(f)
	{
		isr_t handler = f->handler;
		if(handler && !f->block)
			handler(regs);
		f=f->next;
	}
	if(current_task && clear_regs)
		current_task->regs=0;
#if CONFIG_SMP
	lapic_eoi();
#endif
}

void int_sys_init()
{
	memset(&interrupt_handlers, 0, sizeof(isr_t)*256);
#if CONFIG_MODULES
	add_kernel_symbol(register_interrupt_handler);
	add_kernel_symbol(unregister_interrupt_handler);
	add_kernel_symbol(get_interrupt_handler);
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
	total_len += proc_append_buffer(buf, "ISR | HANDLERS | BLOCKED HANDLERS |            COUNT\n", total_len, -1, off, len);
	for(i=0;i<128;i++)
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
			sprintf(t, "%3d | %8d | %16d | %16d\n", i, num_h, num_h-num_w, int_count[i]);
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
