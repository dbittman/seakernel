#include <types.h>
#include <kernel.h>
#include <cpu.h>
#include <task.h>
#include <mutex.h>
#include <elf.h>
#include <atomic.h>
cpu_t *primary_cpu=0;
#if CONFIG_SMP
cpu_t cpu_array[CONFIG_MAX_CPUS];
unsigned cpu_array_num=0;
#endif
cpu_t priamry_cpu_data;

extern mutex_t ipi_mutex;
void init_lapic(int);

void cpuid_get_features(cpuid_t *cpuid)
{
	int eax, ebx, ecx, edx;
	eax = 0x01;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	/* if no LAPIC is present, the INIT IPI will fail. */
	cpuid->features_edx = edx;
	cpuid->features_ecx = ecx;
	cpuid->stepping =    (eax & 0x0F);
	cpuid->model =       ((eax >> 4) & 0x0F);
	cpuid->family =      ((eax >> 8) & 0x0F);
	cpuid->type =        ((eax >> 12) & 0x03);
	cpuid->cache_line_size =     ((ebx >> 8) & 0xFF) * 8; /* cache_line_size * 8 = size in bytes */
	cpuid->logical_processors =  ((ebx >> 16) & 0xFF);    /* # logical cpu's per physical cpu */
	cpuid->lapic_id =            ((ebx >> 24) & 0xFF);    /* Local APIC ID */
} 

void cpuid_get_cpu_brand(cpuid_t *cpuid)
{
	int eax, ebx, ecx, edx;
	eax = 0x80000002;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	cpuid->cpu_brand[48] = 0;     /* init cpu_brand to null-terminate the string */
	char *ccb = cpuid->cpu_brand;
	*(int*)(ccb + 0 ) = (int)eax;
	*(int*)(ccb + 4 ) = ebx;
	*(int*)(ccb + 8 ) = ecx;
	*(int*)(ccb + 12) = edx;
	eax = 0x80000003;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	*(int*)(ccb + 16) = eax;
	*(int*)(ccb + 20) = ebx;
	*(int*)(ccb + 24) = ecx;
	*(int*)(ccb + 28) = edx;
	eax = 0x80000004;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	*(int*)(cpuid->cpu_brand + 32) = eax;
	*(int*)(cpuid->cpu_brand + 36) = ebx;
	*(int*)(cpuid->cpu_brand + 40) = ecx;
	*(int*)(cpuid->cpu_brand + 44) = edx;
	printk(KERN_DEBUG, "\tCPU Brand: %s \n", cpuid->cpu_brand);
}

void parse_cpuid(cpu_t *me)
{
	cpuid_t cpuid;
	int eax, ebx, ecx, edx;
	eax = 0x00;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	cpuid.max_basic_input_val = eax;
	memset(cpuid.manufacturer_string, 0, 13);
	char *ccb = cpuid.manufacturer_string;
	*(int*)(ccb + 0) = ebx;
	*(int*)(cpuid.manufacturer_string + 4) = edx;
	*(int*)(cpuid.manufacturer_string + 8) = ecx;
	printk(KERN_DEBUG, "[cpu]: CPUID: ");
	printk(KERN_DEBUG, "%s\n", cpuid.manufacturer_string);
	if(cpuid.max_basic_input_val >= 1)
		cpuid_get_features(&cpuid);
	eax = 0x80000000;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	cpuid.max_ext_input_val = eax; 
	if((unsigned int)cpuid.max_ext_input_val >= 0x80000004)
		cpuid_get_cpu_brand(&cpuid);
	memcpy(&(me->cpuid), &cpuid, sizeof(me->cpuid));
}
#if CONFIG_SMP

cpu_t *get_cpu(int id)
{
	return &cpu_array[id];
}

cpu_t *add_cpu(cpu_t *c)
{
	if(cpu_array_num >= CONFIG_MAX_CPUS)
		return 0;
	memcpy(&cpu_array[cpu_array_num], c, sizeof(cpu_t));
	mutex_create((mutex_t *)&(cpu_array[cpu_array_num].lock), MT_NOSCHED);
	return &cpu_array[cpu_array_num++];
}

int probe_smp();
#endif

int set_int(unsigned new)
{
	/* need to make sure we don't get interrupted... */
	asm("cli");
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	unsigned old = cpu ? cpu->flags&CPU_INTER : 0;
	if(!new) {
		asm("cli");
		if(cpu) cpu->flags &= ~CPU_INTER;
	} else if(!cpu || cpu->flags&CPU_RUNNING) {
		asm("sti");
		if(cpu) cpu->flags |= CPU_INTER;
	}
	return old;
}

void set_cpu_interrupt_flag(int flag)
{
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	if(!cpu) return;
	if(flag)
		cpu->flags |= CPU_INTER;
	else
		cpu->flags &= ~CPU_INTER;
}

int get_cpu_interrupt_flag()
{
	cpu_t *cpu = current_task ? current_task->cpu : 0;
	return (cpu ? (cpu->flags&CPU_INTER) : 0);
}

void init_main_cpu()
{
#if CONFIG_SMP
	mutex_create(&ipi_mutex, MT_NOSCHED);
	memset(cpu_array, 0, sizeof(cpu_t) * CONFIG_MAX_CPUS);
	cpu_array_num = 0;
	int res = probe_smp();
	if(!(kernel_state_flags & KSF_CPUS_RUNNING))
		primary_cpu = &cpu_array[0];
	if(!primary_cpu)
		primary_cpu = &priamry_cpu_data;
	load_tables_ap(primary_cpu);
	init_lapic(1);
	calibrate_lapic_timer(1000);
	init_ioapic();
 	if(res) {
		set_ksf(KSF_SMP_ENABLE);
	} else
		kprintf("[smp]: error in init code, disabling SMP support\n");
#else
	primary_cpu = &priamry_cpu_data;
	load_tables_ap(primary_cpu);
#endif
	assert(primary_cpu);
	set_int(0);
	primary_cpu->flags = CPU_UP;
	printk(KERN_MSG, "Initializing CPU...\n");
	parse_cpuid(primary_cpu);
	setup_fpu(primary_cpu);
	init_sse(primary_cpu);
	primary_cpu->flags |= CPU_RUNNING;
	printk(KERN_EVERY, "done\n");
	initAcpi();
	mutex_create((mutex_t *)&primary_cpu->lock, MT_NOSCHED);
#if CONFIG_MODULES
	_add_kernel_symbol((unsigned)(cpu_t *)primary_cpu, "primary_cpu");
	add_kernel_symbol(set_int);
#endif
}
