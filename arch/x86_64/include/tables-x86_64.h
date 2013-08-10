#ifndef TABLES_x86_64_H
#define TABLES_x86_64_H
#include <types.h>
#include <tss-x86_64.h>
#include <config.h>

/* in 64 bit mode, the TSS takes up 16 bytes (so we need an extra descriptor) */
#define NUM_GDT_ENTRIES 7

struct gdt_entry_struct
{
	u16int limit_low;
	u16int base_low;
	u8int  base_middle; 
	u8int  access;
	u8int  granularity;
	u8int  base_high; 
} __attribute__((packed));

typedef struct gdt_entry_struct gdt_entry_t;

struct gdt_ptr_struct
{
	u16int limit;
	u64int base;
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

struct idt_entry_struct
{
	u16int base_lo;
	u16int sel;
	u8int  always0;
	u8int  flags;
	u16int base_mid;
	u32int base_high;
	u32int _always0;
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct
{
	u16int limit;
	u64int base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

#include <tables-x86_common.h>

#endif
