#ifndef TABLES_x86_H
#define TABLES_x86_H
#include <sea/types.h>
#include <sea/cpu/tss-x86.h>
#include <sea/config.h>
#define NUM_GDT_ENTRIES 6
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
	u32int base;
} __attribute__((packed));

typedef struct gdt_ptr_struct gdt_ptr_t;

struct idt_entry_struct
{
	u16int base_lo;
	u16int sel;
	u8int  always0;
	u8int  flags;
	u16int base_hi; 
} __attribute__((packed));

typedef struct idt_entry_struct idt_entry_t;

struct idt_ptr_struct
{
	u16int limit;
	u32int base;
} __attribute__((packed));

typedef struct idt_ptr_struct idt_ptr_t;

#include <sea/cpu/tables-x86_common.h>

#endif
