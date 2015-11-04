#include <sea/kernel.h>
#include <sea/vsprintf.h>

#include <sea/debugger.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/processor.h>
#include <sea/tm/thread.h>
#include <sea/tm/process.h>
#include <sea/errno.h>

static int debugger_get_tokens(int maxtoks, int maxlen, char tokens[16][64])
{
	for(int tok=0;tok<maxtoks;tok++) {
		memset(tokens[tok], 0, maxlen);
		if(tok + 1 < maxtoks)
			memset(tokens[tok+1], 0, maxlen);
		for(int i=0;i<maxlen;i++) {
			char key = 0;
			while(1) {
				key = arch_debugger_getkey();
				printk_safe(5, "%c", key);
				if(!i && key == ' ')
					continue;
				if(key == ' ')
					break;
				if(key)
					break;
				cpu_pause();
			}
			if(key == ' ') {
				tokens[tok][i] = 0;
				break;
			}
			else if(key == '\n') {
				return tok + (i == 0 ? 0 : 1);
			} else {
				tokens[tok][i] = key;
			}
		}
	}
	return maxtoks;
}

static void __print_process(struct process *proc)
{
	printk_safe(5, "  refs:         %d\n", proc->refs);
	printk_safe(5, "  magic:        %x\n", proc->magic);
	printk_safe(5, "  flags:        %x\n", proc->flags);
	printk_safe(5, "  heap_start:   %x\n", proc->heap_start);
	printk_safe(5, "  heap_end:     %x\n", proc->heap_end);
	printk_safe(5, "  ppid:         %d\n", proc->parent ? proc->parent->pid : 0);
	printk_safe(5, "  thread_count: %d\n", proc->thread_count);
	printk_safe(5, "  context-v:    %x\n", proc->vmm_context.root_virtual);
	printk_safe(5, "  context-p:    %x\n", proc->vmm_context.root_physical);
	printk_safe(5, "  command:      %s\n", proc->command ? proc->command : "(null)");
}

static void __print_thread(struct thread *thr, int trace)
{
	printk_safe(5, "    refs:               %d\n", thr->refs);
	printk_safe(5, "    pid:                %d\n", thr->process->pid);
	printk_safe(5, "    state:              %d\n", thr->state);
	printk_safe(5, "    flags:              %x\n", thr->flags);
	printk_safe(5, "    usermode_stack_end: %x\n", thr->usermode_stack_end);
	printk_safe(5, "    magic:              %x\n", thr->magic);
	printk_safe(5, "    cpuid:              %d\n", thr->cpuid);
	printk_safe(5, "    system:             %d\n", thr->system);
	printk_safe(5, "    interrupt_level:    %d\n", thr->interrupt_level);
	printk_safe(5, "    kernel_stack:       %x\n", thr->kernel_stack);
	printk_safe(5, "    stack_pointer:      %x\n", thr->stack_pointer);
	printk_safe(5, "    sig_mask:           %x\n", thr->sig_mask);
	printk_safe(5, "    signals_pending:    %x\n", thr->signals_pending);
	printk_safe(5, "    signal:             %d\n", thr->signal);
	printk_safe(5, "    blocklist:          %x\n", thr->blocklist);
	if(thr->regs) {
		printk_safe(5, "    regs->int_no        %d\n", thr->regs->int_no);
		printk_safe(5, "    regs->eip           %x\n", thr->regs->eip);
	}
	if(trace && thr == current_thread) {
		cpu_print_stack_trace(20);
	} else if(trace) {
		struct thread_switch_context tsc;
		mm_context_read(&thr->process->vmm_context, &tsc, thr->stack_pointer, sizeof(struct thread_switch_context));
		addr_t bp = tm_thread_context_basepointer(&tsc);
		printk_safe(5, "    tsc->base_pointer:  %x\n", bp);
		if(bp < thr->kernel_stack || bp > thr->kernel_stack + KERN_STACK_SIZE) {
			printk_safe(5, "    (unable to trace due to unknown base pointer)\n");
		} else {
			cpu_print_stack_trace_alternate(thr, (addr_t *)bp);
		}
	}
}

static void debugger_procs(char tokens[16][64])
{
	struct process *proc;

	if(tokens[1][0] >= '0' && tokens[1][0] <= '9') {
		int id = strtoint(tokens[1]);
		proc = tm_process_get(id);
		if(!proc) {
			printk_safe(5, "no such process %d\n", id);
			return;
		}
		printk_safe(5, "process %d\n", proc->pid);
		__print_process(proc);
		if(!strcmp(tokens[2], "t")) {
			struct linkedentry *tn;
			struct thread *thread;
			for(tn = linkedlist_iter_start(&proc->threadlist);
					tn != linkedlist_iter_end(&proc->threadlist);
					tn = linkedlist_iter_next(tn)) {
				thread = linkedentry_obj(tn);

				printk_safe(5, " * thread %d\n", thread->tid);
				__print_thread(thread, 0);
			}
		}
		tm_process_put(proc);
	} else {
		struct linkedentry *node;
		for(node = linkedlist_iter_start(process_list);
				node != linkedlist_iter_end(process_list);
				node = linkedlist_iter_next(node)) {
			proc = linkedentry_obj(node);
			printk_safe(5, "process %d\n", proc->pid);
			__print_process(proc);
			if(!strcmp(tokens[1], "t")) {
				struct linkedentry *tn;
				struct thread *thread;
				for(tn = linkedlist_iter_start(&proc->threadlist);
						tn != linkedlist_iter_end(&proc->threadlist);
						tn = linkedlist_iter_next(tn)) {
					thread = linkedentry_obj(tn);
					printk_safe(5, " * thread %d\n", thread->tid);
					__print_thread(thread, 0);
				}
			}
		}
	}
}

static void __fn_hash_thread(struct hashelem *ent)
{
	struct thread *thread = ent->ptr;
	printk_safe(5, "thread %d\n", thread->tid);
	__print_thread(thread, 0);
}

static void debugger_threads(char tokens[16][64])
{
	int i=0;
	struct thread *thread;
	if(tokens[1][0] >= '0' && tokens[1][0] <= '9') {
		pid_t id = strtoint(tokens[1]);
		thread = tm_thread_get(id);
		if(!thread) {
			printk_safe(5, "no such thread %d\n", id);
			return;
		}
		printk_safe(5, "thread %d\n", thread->tid);
		__print_thread(thread, 1);
		tm_thread_put(thread);
	} else {
		hash_map(thread_table, __fn_hash_thread);
	}
}

static void debugger_cpus(char tokens[16][64])
{
	for(int i=0;i<CONFIG_MAX_CPUS;i++) {
		struct cpu *cpu = cpu_get(i);
		if(!cpu || !(cpu->flags & CPU_UP))
			break;
		printk_safe(5, "cpu %d\n", i);
		printk_safe(5, "    flags:             %x\n", cpu->flags);
		printk_safe(5, "    preempt_disable:   %d\n", cpu->preempt_disable);
	}
}

struct command {
	char *str;
	void (*fn)(char tokens[16][64]);
};

static struct command commands[3] = {
	{ .str = "thread", .fn = debugger_threads },
	{ .str = "proc", .fn = debugger_procs },
	{ .str = "cpus", .fn = debugger_cpus },
};

void debugger_enter(void)
{
#if CONFIG_SMP
	/* panic already does this */
	if(!(kernel_state_flags & KSF_PANICING)) {
		/* tell the other processors to halt */
		cpu_send_ipi(CPU_IPI_DEST_OTHERS, IPI_PANIC, 0);
		int timeout = 100000;
		while(cpu_get_num_halted_processors() 
				< cpu_get_num_secondary_processors() && --timeout)
			cpu_pause();
	}
#endif
	set_ksf(KSF_DEBUGGING);
	int old = cpu_interrupt_set(0);
	cpu_disable_preemption();
	printk_safe(5, "\n\n---Entered Kernel Debugger (current_thread = %d)---\n", current_thread->tid);

	char tokens[16][64];
	for(;;) {
		printk_safe(5, "debugger> ");
		int toks = debugger_get_tokens(16, 64, tokens);
		if(!toks)
			continue;
		if(!strcmp(tokens[0], "continue"))
			break;
		for(int i=0;i<3;i++) {
			if(!strcmp(commands[i].str, tokens[0]))
				commands[i].fn(tokens);
		}
	}

	printk_safe(5, "---Attempting to restore previous state---\n");
	if(cpu_get_num_secondary_processors() > 0)
		printk_safe(5, "WARNING - CPUs other than %d will remain halted! Processes running on them may not"
				"be able to resume!\n", __current_cpu->knum);
	if(kernel_state_flags & KSF_PANICING)
		printk_safe(5, "WARNING - Continuing execution after kernel panic! Kernel state might be bogus!\n");
	printk_safe(5, "\n");
	/* restore state */
	unset_ksf(KSF_DEBUGGING);
	cpu_enable_preemption();
	cpu_interrupt_set(old);
}

