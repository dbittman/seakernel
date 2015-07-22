#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#include <sea/vsprintf.h>
#include <sea/tty/terminal.h>
#include <sea/string.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif

#define CPUID(cmd, a, b, c, d) __asm__ __volatile__ ("cpuid;"\
		:"=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(cmd));

static void cpuid_get_features(cpuid_t *cpuid)
{
	long int eax, ebx, ecx, edx;
	eax = 0x01;
	CPUID(eax, eax, ebx, ecx, edx);
	/* if no LAPIC is present, the INIT IPI will fail. */
	printk(5, ":: %x\n", edx);
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

static void cpuid_cpu_get_brand(cpuid_t *cpuid)
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
	cpuid_t cpuid;
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
	if((unsigned int)cpuid.max_ext_input_val >= 0x80000004)
		cpuid_cpu_get_brand(&cpuid);
	memcpy(&(me->cpuid), &cpuid, sizeof(me->cpuid));
}

extern void load_tables();
void arch_cpu_early_init(void)
{
	load_tables();
}


