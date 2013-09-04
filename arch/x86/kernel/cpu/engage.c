/* c-entry point for application processors, and their subsequent
initialization */
#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps-x86.h>

void load_tables_ap();
void set_lapic_timer(unsigned tmp);
void init_lapic(int);
addr_t lapic_addr;

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
	/* final part: set the current_task pointer to 'me', and set the 
	 * task flags that allow the cpu to start executing */
	page_directory[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)me;
	((cpu_t *)(me->cpu))->flags |= CPU_TASK;
	me->system = -1;
	set_int(1);
	/* wait until we have tasks to run */
	for(;;) 
		schedule();
}

/* it's important that this doesn't get inlined... */
__attribute__ ((noinline)) void cpu_stage1_init(unsigned apicid)
{
	/* get the cpu again... */
	cpu_t *cpu = get_cpu(apicid);
	cpu->flags |= CPU_UP;
	/* call the CPU features init code */
	parse_cpuid(cpu);
	setup_fpu(cpu);
	init_sse(cpu);
	cpu->flags |= CPU_RUNNING;
	set_boot_flag(0xFFFFFFFF);
	while(!(kernel_state_flags & KSF_SMP_ENABLE)) asm("cli");
	init_lapic(0);
	set_lapic_timer(lapic_timer_start);
	/* now we need to wait up the memory manager is all set up */
	while(!(kernel_state_flags & KSF_MMU)) asm("cli");
	/* load in the directory provided and enable paging! */
	__asm__ volatile ("mov %0, %%cr3" : : "r" (cpu->kd_phys));
	unsigned cr0temp;
	
	enable_paging();
	/* map in the real stack */
	unsigned i;
	for(i=(unsigned int)STACK_LOCATION+STACK_SIZE; i >= (unsigned int)STACK_LOCATION - STACK_SIZE*2;i -= 0x1000) {
		vm_map(i, pm_alloc_page(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
		memset((void *)i, 0, 0x1000);
	}
	
	printk(0, "[cpu%d]: waiting for tasking...\n", apicid);
	while(!kernel_task) asm("cli");
	printk(0, "[cpu%d]: enable tasks...\n", apicid);
	/* initialize tasking for this CPU */
	task_t *task = task_create();
	task->pid = add_atomic(&next_pid, 1)-1;
	task->pd = (page_dir_t *)cpu->kd;
	task->stack_end=STACK_LOCATION;
	task->priority = 1;
	
	cpu->active_queue = tqueue_create(0, 0);
	tqueue_insert(primary_queue, (void *)task, task->listnode);
	tqueue_insert(cpu->active_queue, (void *)task, task->activenode);
	cpu->cur = cpu->ktask = task;
	task->cpu = cpu;
	mutex_create(&cpu->lock, MT_NOSCHED);
	cpu->numtasks=1;
	task->thread = thread_data_create();
	set_kernel_stack(&cpu->tss, task->kernel_stack + (KERN_STACK_SIZE - STACK_ELEMENT_SIZE));
	add_atomic(&running_processes, 1);
	/* set up the real stack, and call cpu_k_task_entry with a pointer to this cpu's ktask as 
	 * the argument */
	asm(" \
		mov %0, %%eax; \
		mov %1, %%ebx; \
		mov %2, %%esp; \
		mov %2, %%ebp; \
		push %%ebx; \
		call *%%eax;" :: "r" (cpu_k_task_entry),"r"(task),"r"(STACK_LOCATION + STACK_SIZE - STACK_ELEMENT_SIZE));
	/* we'll never get here */
}

/* C-side CPU entry code. Called from the assembly handler */
void cpu_entry(void)
{
	/* get the ID and the cpu struct so we can set a private stack */
	int apicid = get_boot_flag();
	cpu_t *cpu = get_cpu(apicid);
	/* load up the pmode gdt, tss, and idt */
	load_tables_ap(cpu);
	/* set up our private temporary tack */
	asm("mov %0, %%esp" : : "r" (cpu->stack + (CPU_STACK_TEMP_SIZE - STACK_ELEMENT_SIZE)));
	asm("mov %0, %%ebp" : : "r" (cpu->stack + (CPU_STACK_TEMP_SIZE - STACK_ELEMENT_SIZE)));
	cpu_stage1_init(get_boot_flag());
}

int boot_cpu(unsigned id, unsigned apic_ver)
{
	int apicid = id, success = 1, to;
	unsigned bootaddr, accept_status;
	unsigned bios_reset_vector = BIOS_RESET_VECTOR;
	set_int(0);
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
	send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	/* de-assert INIT IPI */
	send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	if (apic_ver >= APIC_VER_NEW) {
		int i;
		for (i = 1; i <= 2; i++) {
			send_ipi(LAPIC_ICR_SHORT_DEST, apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			delay_sleep(1);
		}
	}
	to = 0;
	while ((get_boot_flag() != 0xFFFFFFFF) && to++ < 100)
		delay_sleep(10);
	/* cpu didn't boot up...:( */
	if (to >= 100)
		success = 0;
	set_int(0);
	/* clear the APIC error register */
	LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;
	return success;
}

#endif
