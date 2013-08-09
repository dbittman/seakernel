#ifndef _CPU_X86_64_H
#define _CPU_X86_64_H

#define CPU_EXT_FEATURES_GBPAGE (1 << 26)

typedef struct {
	char manufacturer_string[13];
	int max_basic_input_val;
	int max_ext_input_val;
	int features_ecx, features_edx;
	int ext_features_ecx, ext_features_edx;
	char stepping, model, family, type; 
	char cache_line_size, logical_processors, lapic_id;
	char cpu_brand[49];
} cpuid_t;

extern addr_t lapic_addr;
#define LAPIC_READ(x)  (*((volatile unsigned *) (lapic_addr+(x))))
#define LAPIC_WRITE(x, y)   \
(*((volatile unsigned *) (lapic_addr+(x))) = (y))

void init_ioapic();
void lapic_eoi();

extern int trampoline_start(void);
extern int trampoline_end(void);
extern int pmode_enter(void);
extern int pmode_enter_end(void);
extern int rm_gdt(void);
extern int rm_gdt_end(void);
extern int rm_gdt_pointer(void);

#define RM_GDT_SIZE 0x18
#define GDT_POINTER_SIZE 0x4
#define RM_GDT_START 0x7100
#define BOOTFLAG_ADDR 0x7200

void add_ioapic(addr_t address, int type, int id, int int_start);

#endif
