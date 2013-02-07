#include <types.h>
#include <kernel.h>
#include <cpu.h>
#include <task.h>
#include <mutex.h>
#include <elf.h>
cpu_t primary_cpu;
#if CONFIG_SMP
cpu_t *cpu_list;
mutex_t cpulist_lock;
#endif

void cpuid_get_features(cpuid_t *cpuid)
{
	int eax, ebx, ecx, edx;
	eax = 0x01;
	asm("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(eax));
	cpuid->features_edx = edx;
	cpuid->features_ecx = ecx;
	cpuid->stepping =    (eax & 0x0F);
	cpuid->model =       ((eax >> 4) & 0x0F);
	cpuid->family =      ((eax >> 8) & 0x0F);
	cpuid->type =        ((eax >> 12) & 0x03);
	cpuid->cache_line_size =     ((ebx >> 8) & 0xFF) * 8; /* cache_line_size * 8 = size in bytes */
	cpuid->logical_processors =  ((ebx >> 16) & 0xFF);    /* # logical cpu's per physical cpu */
	cpuid->lapic_id =            ((ebx >> 24) & 0xFF);    /* Local APIC ID */
	printk(KERN_EVERY, "\tFamily: 0x%X | Model: 0x%X | Stepping: 0x%X | Type: 0x%X \n",  
	       cpuid->family, 
	cpuid->model, 
	cpuid->stepping, 
	cpuid->type);
	printk(KERN_EVERY, "\tCache Line Size: %u bytes | Local APIC ID: 0x%X \n", 
	       cpuid->cache_line_size, 
	cpuid->lapic_id);
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
void remove_cpu(cpu_t *c)
{
	mutex_on(&cpulist_lock);
	assert(c->prev);
	c->prev->next = c->next;
	if(c->next)
		c->next->prev = c->prev;
	mutex_off(&cpulist_lock);
}

void delete_cpu(cpu_t *c)
{
	remove_cpu(c);
	kfree(c);
}

void add_cpu(cpu_t *c)
{
	assert(cpu_list);
	mutex_on(&cpulist_lock);
	cpu_t *t = cpu_list->next;
	cpu_list->next = c;
	c->prev = cpu_list;
	if(t)
		t->prev = c;
	c->next = t;
	mutex_off(&cpulist_lock);
}

cpu_t *get_cpu(int id)
{
	cpu_t *c = cpu_list;
	while(c) {
		if(c->apicid == id)
			break;
		c=c->next;
	}
	return c;
}

int probe_smp();
#endif
void init_main_cpu()
{
#if CONFIG_SMP
	cpu_list = &primary_cpu;
	memset(cpu_list, 0, sizeof(*cpu_list));
#endif
	primary_cpu.flags = CPU_UP | CPU_RUNNING;
#if CONFIG_SMP
	create_mutex(&cpulist_lock);
#endif
	printk(KERN_MSG, "Initializing CPU...\n");
	parse_cpuid(&primary_cpu);
	setup_fpu(&primary_cpu);
	init_sse(&primary_cpu);
	printk(KERN_EVERY, "done\n");
	initAcpi();
#if CONFIG_SMP
	probe_smp();
#endif
#if CONFIG_MODULES
	_add_kernel_symbol((unsigned)(cpu_t *)&primary_cpu, "primary_cpu");
#endif
}
