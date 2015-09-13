/* c-entry point for application processors, and their subsequent
initialization */
#include <sea/config.h>
#if CONFIG_SMP
#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/mm/vmm.h>
#include <stdatomic.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/cpu/features-x86_common.h>
#include <sea/cpu/cmos-x86_common.h>
#include <sea/asm/system.h>
#include <sea/cpu/interrupt.h>
#include <sea/tm/timing.h>
void load_tables_ap();
void set_lapic_timer(unsigned tmp);
void init_lapic(int);
addr_t lapic_addr = 0;

static inline void set_boot_flag(unsigned x)
{
	*(unsigned *)(BOOTFLAG_ADDR + PHYS_PAGE_MAP) = x;
}

static inline  unsigned get_boot_flag(void)
{
	return *(unsigned *)(BOOTFLAG_ADDR + PHYS_PAGE_MAP);
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
	__asm__ __volatile__ ("mov %0, %%cr3" : : "r" (kernel_context.root_physical) : "memory");
	__asm__ __volatile__ ("mov %%cr0, %%rax; or $0x80000000, %%eax; mov %%rax, %%cr0":::"rax", "memory");
	__asm__ __volatile__ ("mov %0, %%rsp" : : "r" (cpu->stack + KERN_STACK_SIZE) : "memory");
	__asm__ __volatile__ ("mov %0, %%rbp" : : "r" (cpu->stack + KERN_STACK_SIZE) : "memory");
	__asm__ __volatile__ ("jmp cpu_stage1_init" ::: "memory");
}

int boot_cpu(struct cpu *cpu)
{
	int apicid = cpu->snum, success = 1, to;
	addr_t bootaddr_phys, bootaddr_virt, accept_status;
	addr_t bios_reset_vector = BIOS_RESET_VECTOR;

	printk(1, "[smp]: poking cpu %d\n", apicid);

	/* TODO: this should be its own function? We do it in fork too... */
	struct thread *thread = kmalloc(sizeof(struct thread));
	

	cpu->active_queue = tqueue_create(0, 0);
	cpu->numtasks=1;
	ticker_create(&cpu->ticker, 0);
	workqueue_create(&cpu->work, 0);
	thread->refs = 1;
	cpu->idle_thread = thread;
	thread->process = kernel_process; /* we have to do this early, so that the vmm system can use the lock... */
	thread->state = THREADSTATE_RUNNING;
	thread->tid = tm_thread_next_tid();
	thread->magic = THREAD_MAGIC;
	workqueue_create(&thread->resume_work, 0);
	mutex_create(&thread->block_mutex, MT_NOSCHED);
	hash_table_set_entry(thread_table, &thread->tid, sizeof(thread->tid), 1, thread);
	
	tm_thread_add_to_process(thread, kernel_process);
	tm_thread_reserve_stacks(thread);
	cpu->stack = thread->kernel_stack;
	size_t kms_page_size = mm_page_size_closest(KERN_STACK_SIZE);
	for(int i = 0;i<((KERN_STACK_SIZE-1) / kms_page_size)+1;i++) {
		addr_t phys = mm_physical_allocate(kms_page_size, false);
		bool r = mm_virtual_map(thread->kernel_stack + i * kms_page_size,
				phys,
				PAGE_PRESENT | PAGE_WRITE, kms_page_size);
		if(!r)
			mm_physical_deallocate(phys);
	}
	*(struct thread **)(thread->kernel_stack) = thread;
	tm_thread_add_to_cpu(thread, cpu);
	atomic_fetch_add(&running_threads, 1);
	cpu_disable_preemption();
	/* choose this as the bios reset vector */
	bootaddr_phys = 0x7000;
	bootaddr_virt = bootaddr_phys + PHYS_PAGE_MAP;
	addr_t sz = (addr_t)trampoline_end - (addr_t)trampoline_start;
	/* copy in the 16-bit real mode entry code */
	memcpy((void *)bootaddr_virt, (void *)((addr_t)trampoline_start + PHYS_PAGE_MAP), sz);
	/* to switch into protected mode, the CPU needs access to a GDT, so
	 * give it one... */
	memcpy((void *)(RM_GDT_START+GDT_POINTER_SIZE + PHYS_PAGE_MAP), (void *)((addr_t)rm_gdt + PHYS_PAGE_MAP), RM_GDT_SIZE);
	memcpy((void *)(RM_GDT_START + PHYS_PAGE_MAP), (void *)((addr_t)rm_gdt_pointer + PHYS_PAGE_MAP), GDT_POINTER_SIZE);
	/* copy down the pmode_enter code as well, since this gets called
	 * from real mode */
	memcpy((void *)(RM_GDT_START+RM_GDT_SIZE+GDT_POINTER_SIZE + PHYS_PAGE_MAP), (void *)((addr_t)pmode_enter + PHYS_PAGE_MAP), (addr_t)pmode_enter_end - (addr_t)pmode_enter);
	/* the CPU will look here to figure out it's APICID */
	set_boot_flag(apicid);
	/* set BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, CMOS_RESET_JUMP);
	*((volatile unsigned *) ((addr_t)bios_reset_vector + PHYS_PAGE_MAP)) = ((bootaddr_phys & 0xFF000) << 12);
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);
	/* assert INIT IPI */
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);
	tm_thread_delay_sleep(ONE_MILLISECOND * 10);
	/* de-assert INIT IPI */
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	tm_thread_delay_sleep(ONE_MILLISECOND * 10);
		int i;
		for (i = 1; i <= 2; i++) {
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_DM_SIPI | ((bootaddr_phys >> 12) & 0xFF));
			tm_thread_delay_sleep(ONE_MILLISECOND);
		}
	to = 0;
	while ((get_boot_flag() != 0xFFFFFFFF) && to++ < 100)
		tm_thread_delay_sleep(ONE_MILLISECOND * 10);
	/* cpu didn't boot up...:( */
	if (to >= 100)
		success = 0;
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);
	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) ((addr_t)bios_reset_vector + PHYS_PAGE_MAP)) = 0;
	cpu_enable_preemption();
	return success;
}

#endif

