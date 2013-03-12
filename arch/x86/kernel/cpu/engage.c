/* c-entry point for application processors, and their subsequent
initialization */
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps.h>

static void set_boot_flag(unsigned x)
{
	*(unsigned *)(BOOTFLAG_ADDR) = x;
}

static unsigned get_boot_flag()
{
	return *(unsigned *)(BOOTFLAG_ADDR);
}

void cpu_k_task_entry(task_t *me)
{
	page_directory[PAGE_DIR_IDX(SMP_CUR_TASK / PAGE_SIZE)] = (unsigned)me;
	((cpu_t *)(me->cpu))->cur = me;
	((cpu_t *)(me->cpu))->flags |= CPU_TASK;
	for(;;);
	//sti();
	for(;;) schedule();
}

void load_tables_ap();
void set_lapic_timer(unsigned tmp);
extern unsigned lapic_timer_start;
void init_lapic();
void cpu_entry(void)
{
	int myid = get_boot_flag();
	cpu_t *cpu = get_cpu(myid);
	asm("mov %0, %%esp" : : "r" (cpu->stack + 1000));
	asm("mov %0, %%ebp" : : "r" (cpu->stack + 1000));
	load_tables_ap();
	myid = get_boot_flag();
	cpu = get_cpu(myid);
	parse_cpuid(cpu);
	setup_fpu(cpu);
	init_sse(cpu);
	init_lapic();
	cpu->flags |= CPU_UP;
	set_boot_flag(0xFFFFFFFF);
	while(!mmu_ready && !cpu->kd && !kernel_dir); 
	__asm__ volatile ("mov %0, %%cr3" : : "r" (cpu->kd_phys));
	unsigned cr0temp;
	enable_paging();
	
	unsigned i;
	for(i=(unsigned int)STACK_LOCATION+STACK_SIZE; i >= (unsigned int)STACK_LOCATION - STACK_SIZE*2;i -= 0x1000) {
		vm_map(i, pm_alloc_page(), PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
		memset((void *)i, 0, 0x1000);
	}
	
	printk(0, "[cpu%d]: waiting for tasking...\n", myid);
	while(!kernel_task) cli();
	printk(0, "[cpu%d]: enable tasks...\n", myid);
	task_t *task = (task_t *)kmalloc(sizeof(task_t));
	task->pid = add_atomic(&next_pid, 1)-1;
	task->pd = (page_dir_t *)cpu->kd;
	task->stack_end=STACK_LOCATION;
	task->kernel_stack = kmalloc(KERN_STACK_SIZE+8);
	task->priority = 1;
	task->magic = TASK_MAGIC;
	cpu->active_queue = tqueue_create(0, 0);
	task->listnode = tqueue_insert(primary_queue, (void *)task);
	task->activenode = tqueue_insert(cpu->active_queue, (void *)task);
	cpu->ktask = task;
	task->cpu = cpu;
	asm(" \
		mov %0, %%eax; \
		mov %1, %%ebx; \
		mov %2, %%esp; \
		mov %2, %%ebp; \
		push %%ebx; \
		call *%%eax;" :: "r" (cpu_k_task_entry),"r"(task),"r"(STACK_LOCATION + STACK_SIZE - STACK_ELEMENT_SIZE));
	for(;;);
}

int boot_cpu(unsigned id, unsigned apic_ver)
{
	int apicid = id, success = 1, to;
	unsigned bootaddr, accept_status;
	unsigned bios_reset_vector = BIOS_RESET_VECTOR;
	
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
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	/* de-assert INIT IPI */
	send_ipi(apicid, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);
	delay_sleep(10);
	if (apic_ver >= APIC_VER_NEW) {
		int i;
		for (i = 1; i <= 2; i++) {
			send_ipi(apicid, LAPIC_ICR_DM_SIPI | ((bootaddr >> 12) & 0xFF));
			delay_sleep(1);
		}
	}
	to = 0;
	while ((get_boot_flag(bootaddr) != 0xFFFFFFFF) && to++ < 100)
		delay_sleep(10);
	/* cpu didn't boot up...:( */
	if (to >= 100)
		success = 0;
	/* clear the APIC error register */
	IMPS_LAPIC_WRITE(LAPIC_ESR, 0);
	accept_status = IMPS_LAPIC_READ(LAPIC_ESR);

	/* clean up BIOS reset vector */
	CMOS_WRITE_BYTE(CMOS_RESET_CODE, 0);
	*((volatile unsigned *) bios_reset_vector) = 0;
	return success;
}
