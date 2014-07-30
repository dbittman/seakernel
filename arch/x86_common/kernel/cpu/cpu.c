#include <sea/kernel.h>
#include <sea/cpu/processor.h>
#include <sea/cpu/interrupt.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <sea/cpu/cpu-x86.h>
#else
#include <sea/cpu/cpu-x86_64.h>
#endif

static void cpuid_get_features(cpuid_t *cpuid)
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
	eax = 0x80000001;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	cpuid->ext_features_edx = edx;
	cpuid->ext_features_ecx = ecx;
} 

static void cpuid_cpu_get_brand(cpuid_t *cpuid)
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
		cpuid_cpu_get_brand(&cpuid);
	memcpy(&(me->cpuid), &cpuid, sizeof(me->cpuid));
}

extern void load_tables();
void arch_cpu_early_init()
{
	load_tables();
}


