/* c-entry point for application processors, and their subsequent
initialization */
#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <sea/mutex.h>
#include <cpu.h>
#include <memory.h>
#include <sea/cpu/atomic.h>

void load_tables_ap();
void set_lapic_timer(unsigned tmp);
void init_lapic(int);
addr_t lapic_addr = 0;

static inline void set_boot_flag(unsigned x)
{
	*(unsigned *)(BOOTFLAG_ADDR) = x;
}

static inline  unsigned get_boot_flag()
{
	return *(unsigned *)(BOOTFLAG_ADDR);
}

void cpu_k_task_entry(task_t *me)
{
	cpu_smp_task_idle(me);
}

/* it's important that this doesn't get inlined... */
__attribute__ ((noinline)) void cpu_stage1_init(unsigned apicid)
{
	/* get the cpu again... */
	cpu_t *cpu = cpu_get(apicid);
	cpu->flags |= CPU_UP;
	/* call the CPU features init code */
	parse_cpuid(cpu);
	x86_cpu_init_fpu(cpu);
	x86_cpu_init_sse(cpu);
	
	/* set up a real paging structure */
	pml4_t *initial_pml4 = (pml4_t *)kernel_dir_phys;
	asm("mov %0, %%cr3" :: "r"(initial_pml4));
	pml4_t *new_pml4 = mm_vm_clone((addr_t *)kernel_dir, 0);
	arch_mm_vm_switch_context(new_pml4);
	cpu->kd = new_pml4;
	
	/* initialize tasking for this CPU */
	task_t *task = tm_task_create();
	arch_tm_set_current_task_marker(new_pml4, (addr_t)task);
	task->pd = (page_dir_t *)cpu->kd;
	task->stack_end=STACK_LOCATION;
	task->priority = 1;
	cpu->cur = cpu->ktask = task;
	
	task->cpu = cpu;
	mutex_create(&cpu->lock, MT_NOSCHED);
	cpu->numtasks=1;
	task->thread = tm_thread_data_create();
	set_kernel_stack(&cpu->tss, task->kernel_stack + (KERN_STACK_SIZE - STACK_ELEMENT_SIZE));
	add_atomic(&running_processes, 1);
	cpu->flags |= CPU_RUNNING;
	/* indicate that we're ready for the next processor to run */
	set_boot_flag(0xFFFFFFFF);
	
	while(!(kernel_state_flags & KSF_SMP_ENABLE)) asm("cli; pause");
	init_lapic(0);
	set_lapic_timer(lapic_timer_start);
	
	while(!kernel_task) asm("cli; pause");
	
	task->pid = add_atomic(&next_pid, 1)-1;
	tqueue_insert(primary_queue, (void *)task, task->listnode);
	
	cpu->active_queue = tqueue_create(0, 0);
	tqueue_insert(cpu->active_queue, (void *)task, task->activenode);
	asm("mov %0, %%rsp" : : "r" (STACK_LOCATION + STACK_SIZE - STACK_ELEMENT_SIZE));
	asm("mov %0, %%rbp" : : "r" (STACK_LOCATION + STACK_SIZE - STACK_ELEMENT_SIZE));
	cpu_k_task_entry(current_task);
}

/* C-side CPU entry code. Called from the assembly handler */
void cpu_entry(void)
{
	/* get the ID and the cpu struct so we can set a private stack */
	int apicid = get_boot_flag();
	cpu_t *cpu = cpu_get(apicid);
	/* load up the pmode gdt, tss, and idt */
	load_tables_ap(cpu);
	/* set up our private temporary tack */
	asm("mov %0, %%rsp" : : "r" (cpu->stack + (CPU_STACK_TEMP_SIZE - STACK_ELEMENT_SIZE)));
	asm("mov %0, %%rbp" : : "r" (cpu->stack + (CPU_STACK_TEMP_SIZE - STACK_ELEMENT_SIZE)));
	cpu_stage1_init(get_boot_flag());
}

int boot_cpu(unsigned id, unsigned apic_ver)
{
	int apicid = id, success = 1, to;
	addr_t bootaddr, accept_status;
	addr_t bios_reset_vector = BIOS_RESET_VECTOR;
	interrupt_set(0);
	/* choose this as the bios reset vector */
	bootaddr = 0x7000;
	addr_t sz = (addr_t)trampoline_end - (addr_t)trampoline_start;
	/* copy in the 16-bit real mode entry code */
	memcpy((void *)bootaddr, (void *)trampoline_start, sz);
	/* to switch into protected mode, the CPU needs access to a GDT, so
	 * give it one... */
	memcpy((void *)(RM_GDT_START+GDT_POINTER_SIZE), (void *)rm_gdt, RM_GDT_SIZE);
	memcpy((void *)RM_GDT_START, (void *)rm_gdt_pointer, GDT_POINTER_SIZE);
	/* copy down the pmode_enter code as well, since this gets called
	 * from real mode */
	memcpy((void *)(RM_GDT_START+RM_GDT_SIZE+GDT_POINTER_SIZE), (void *)pmode_enter, (addr_t)pmode_enter_end - (addr_t)pmode_enter);
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
	tm_delay_sleep(10);
	/* de-assert INIT IPI */
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	tm_delay_sleep(10);
	if (apic_ver >= APIC_VER_NEW) {
		int i;
		for (i = 1; i <= 2; i++) {
			x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			tm_delay_sleep(1);
		}
	}
	to = 0;
	while ((get_boot_flag() != 0xFFFFFFFF) && to++ < 100)
		tm_delay_sleep(10);
	/* cpu didn't boot up...:( */
	if (to >= 100)
		success = 0;
	interrupt_set(0);
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;
	return success;
}

#endif
