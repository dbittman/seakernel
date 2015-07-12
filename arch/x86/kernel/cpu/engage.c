/* c-entry point for application processors, and their subsequent
initialization */
#include <sea/config.h>
#if CONFIG_SMP
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/imps-x86.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/cmos-x86_common.h>
#include <sea/cpu/features-x86_common.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/cpu-x86.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/tm/timing.h>
#include <sea/tm/workqueue.h>
void set_lapic_timer(unsigned tmp);
void init_lapic(int);
addr_t lapic_addr=0;

static inline void set_boot_flag(unsigned x)
{
	*(unsigned *)(BOOTFLAG_ADDR) = x;
}

static inline  unsigned get_boot_flag(void)
{
	return *(unsigned *)(BOOTFLAG_ADDR);
}

/* it's important that this doesn't get inlined... */
extern struct process *kernel_process;
__attribute__ ((noinline)) void cpu_stage1_init(void)
{
	/* get the cpu again... */
	struct cpu *cpu = cpu_get_snum(get_boot_flag());
	cpu->flags |= CPU_UP;
	set_boot_flag(0xFFFFFFFF);
	init_lapic(0);
	set_lapic_timer(lapic_timer_start);
	/* call the CPU features init code */
	parse_cpuid(cpu);
	x86_cpu_init_fpu(cpu);
	x86_cpu_init_sse(cpu);
	/* initialize tasking for this CPU */
	cpu_smp_task_idle(cpu);
}

/* C-side CPU entry code. Called from the assembly handler */
void cpu_entry(void)
{
	/* get the ID and the cpu struct so we can set a private stack */
	int apicid = get_boot_flag();
	struct cpu *cpu = cpu_get_snum(apicid);
	/* load up the pmode gdt, tss, and idt */
	load_tables_ap(cpu);
	/* set up our private temporary tack */
	__asm__ __volatile__ ("mov %0, %%cr3" : : "r" (kernel_context.root_physical));
	__asm__ __volatile__ ("mov %%cr0, %%eax; or $0x80000000, %%eax; mov %%eax, %%cr0":::"eax");
	__asm__ __volatile__ ("mov %0, %%esp" : : "r" (cpu->stack + KERN_STACK_SIZE));
	__asm__ __volatile__ ("mov %0, %%ebp" : : "r" (cpu->stack + KERN_STACK_SIZE));
	cpu_stage1_init();
}

int boot_cpu(struct cpu *cpu)
{
	int apicid = cpu->snum, success = 1, to;
	unsigned bootaddr, accept_status;
	unsigned bios_reset_vector = BIOS_RESET_VECTOR;
	printk(1, "[smp]: poking cpu %d\n", apicid);

	cpu->stack = (addr_t)kmalloc_a(KERN_STACK_SIZE);
	cpu->active_queue = tqueue_create(0, 0);
	cpu->numtasks=1;
	ticker_create(&cpu->ticker, 0);
	workqueue_create(&cpu->work, 0);
	struct thread *thread = kmalloc(sizeof(struct thread));
	thread->refs = 1;
	cpu->idle_thread = thread;
	thread->process = kernel_process; /* we have to do this early, so that the vmm system can use the lock... */
	thread->state = THREAD_RUNNING;
	/* TODO: this line was causing weirdness */
	thread->tid = tm_thread_next_tid();
	thread->magic = THREAD_MAGIC;
	thread->kernel_stack = (void *)cpu->stack;
	*(struct thread **)(thread->kernel_stack) = thread;
	tm_thread_add_to_process(thread, kernel_process);
	tm_thread_add_to_cpu(thread, cpu);
	add_atomic(&running_threads, 1);

	/* choose this as the bios reset vector */
	bootaddr = 0x7000;
	unsigned sz = (unsigned)trampoline_end - (unsigned)trampoline_start;
	/* copy in the 16-bit real mode entry code */
	memcpy((void *)bootaddr, (void *)trampoline_start, sz);
	/* to switch into protected mode, the CPU needs access to a GDT, so
	 * give it one... */
	memcpy((void *)(RM_GDT_START+GDT_POINTER_SIZE), (void *)rm_gdt, RM_GDT_SIZE);
	memcpy((void *)RM_GDT_START, (void *)rm_gdt_pointer, GDT_POINTER_SIZE);
	/* copy down the pmode_enter code as well, since this gets called
	 * from real mode */
	memcpy((void *)(RM_GDT_START+RM_GDT_SIZE+GDT_POINTER_SIZE), (void *)pmode_enter, (unsigned)pmode_enter_end - (unsigned)pmode_enter);
	/* the CPU will look here to figure out it's APICID */
	set_boot_flag(apicid);
	/* set BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, CMOS_RESET_JUMP);
	*((volatile unsigned *) bios_reset_vector) = ((bootaddr & 0xFF000) << 12);
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);
	/* assert INIT IPI */
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);
	tm_thread_delay_sleep(ONE_MILLISECOND * 10);
	/* de-assert INIT IPI */
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	tm_thread_delay_sleep(ONE_MILLISECOND * 10);
	//if (apic_ver >= APIC_VER_NEW) { /* TODO */
		int i;
		for (i = 1; i <= 2; i++) {
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			tm_thread_delay_sleep(ONE_MILLISECOND);
		}
	//}
	to = 0;
	while ((get_boot_flag() != 0xFFFFFFFF) && to++ < 100)
		tm_thread_delay_sleep(10 * ONE_MILLISECOND);
	/* cpu didn't boot up...:( */
	if (to >= 100)
		success = 0;
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;
	return success;
}

#endif

