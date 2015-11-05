#ifndef TABLES_x86_64_H
#define TABLES_x86_64_H
#include <sea/types.h>
#include <sea/cpu/tss-x86_64.h>
#include <sea/config.h>

/* in 64 bit mode, the TSS takes up 16 bytes (so we need an extra descriptor) */
#define NUM_GDT_ENTRIES 7

struct gdt_entry_struct
{
	uint16_t limit_low;
	uint16_t base_low;
	uint8_t  base_middle; 
	uint8_t  access;
	uint8_t  granularity;
	uint8_t  base_high; 
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

struct idt_entry_struct
{
	uint16_t base_lo;
	uint16_t sel;
	uint8_t  always0;
	uint8_t  flags;
	uint16_t base_mid;
	uint32_t base_high;
	uint32_t _always0;
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct
{
	uint16_t limit;
	uint64_t base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

#include <sea/cpu/tables-x86_common.h>

#endif
