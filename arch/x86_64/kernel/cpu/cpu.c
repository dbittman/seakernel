#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/cpu-x86_64.h>
#include <sea/tm/process.h>
#include <sea/mutex.h>

#include <sea/cpu/acpi.h>
#include <sea/cpu/features-x86_common.h>
#include <sea/vsprintf.h>
#include <sea/cpu/interrupt.h>
#include <sea/string.h>
extern struct cpu *primary_cpu;
#if CONFIG_SMP
extern struct cpu cpu_array[CONFIG_MAX_CPUS];
extern unsigned cpu_array_num;
#endif
extern struct cpu primary_cpu_data;
void init_lapic(void);
int probe_smp();
void init_acpi();
void arch_cpu_processor_init_1(void)
{
#if CONFIG_SMP
	primary_cpu = &cpu_array[0];
	primary_cpu->knum = 0;
	spinlock_create(&ipi_lock);
	memset(cpu_array, 0, sizeof(struct cpu) * CONFIG_MAX_CPUS);
	cpu_array_num = 1;
	load_tables_ap(primary_cpu);
#else
	primary_cpu = &primary_cpu_data;
	memset(primary_cpu, 0, sizeof(struct cpu));
	load_tables_ap(primary_cpu);
#endif
	assert(primary_cpu);
	cpu_interrupt_set(0);
	primary_cpu->flags = CPU_UP;
	printk(KERN_MSG, "Initializing CPU...\n");
	parse_cpuid(primary_cpu);
	x86_cpu_init_fpu(primary_cpu);
	x86_cpu_init_sse(primary_cpu);
	printk(KERN_EVERY, "done\n");
}

void x86_hpet_init(void);
void arch_cpu_processor_init_2(void)
{
	acpi_init();
	x86_hpet_init();
#if CONFIG_SMP
	probe_smp();
	init_lapic();
	calibrate_lapic_timer(1000);
	init_ioapic();
	set_ksf(KSF_SMP_ENABLE);
#endif
}

#if CONFIG_SMP
int arch_cpu_boot_ap(struct cpu *cpu)
{
	int re = boot_cpu(cpu);
	if(!re) {
		cpu->flags |= CPU_ERROR;
		num_failed_cpus++;
	} else
		num_booted_cpus++;
	return re;
}
#endif

#define CPUID(cmd, a, b, c, d) __asm__ __volatile__ ("cpuid;"\
		:"=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(cmd));

static void cpuid_get_features(struct cpuid *cpuid)
{
	long int eax, ebx, ecx, edx;
	eax = 0x01;
	CPUID(eax, eax, ebx, ecx, edx);
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
	eax = 0x80000001;
	CPUID(eax, eax, ebx, ecx, edx);
	cpuid->ext_features_edx = edx;
	cpuid->ext_features_ecx = ecx;
} 

static void cpuid_cpu_get_brand(struct cpuid *cpuid)
{
	long int eax, ebx, ecx, edx;
	eax = 0x80000002;
	CPUID(eax, eax, ebx, ecx, edx);
	cpuid->cpu_brand[48] = 0;     /* init cpu_brand to null-terminate the string */
	char *ccb = cpuid->cpu_brand;
	*(int*)(ccb + 0 ) = (int)eax;
	*(int*)(ccb + 4 ) = ebx;
	*(int*)(ccb + 8 ) = ecx;
	*(int*)(ccb + 12) = edx;
	eax = 0x80000003;
	CPUID(eax, eax, ebx, ecx, edx);
	*(int*)(ccb + 16) = eax;
	*(int*)(ccb + 20) = ebx;
	*(int*)(ccb + 24) = ecx;
	*(int*)(ccb + 28) = edx;
	eax = 0x80000004;
	CPUID(eax, eax, ebx, ecx, edx);
	*(int*)(cpuid->cpu_brand + 32) = eax;
	*(int*)(cpuid->cpu_brand + 36) = ebx;
	*(int*)(cpuid->cpu_brand + 40) = ecx;
	*(int*)(cpuid->cpu_brand + 44) = edx;
}

void parse_cpuid(struct cpu *me)
{
	struct cpuid cpuid;
	long int eax, ebx, ecx, edx;
	eax = 0x00;
	CPUID(eax, eax, ebx, ecx, edx);
	cpuid.max_basic_input_val = eax;
	memset(cpuid.manufacturer_string, 0, 13);
	char *ccb = cpuid.manufacturer_string;
	*(int*)(ccb + 0) = ebx;
	*(int*)(cpuid.manufacturer_string + 4) = edx;
	*(int*)(cpuid.manufacturer_string + 8) = ecx;
	if(cpuid.max_basic_input_val >= 1)
		cpuid_get_features(&cpuid);
	eax = 0x80000000;
	CPUID(eax, eax, ebx, ecx, edx);
	cpuid.max_ext_input_val = eax; 
	memcpy(&(me->cpuid), &cpuid, sizeof(me->cpuid));
}

extern void load_tables();
void arch_cpu_early_init(void)
{
	load_tables();
}


