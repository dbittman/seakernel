/* handles exporting of kernel symbols */

#include <kernel.h>
#include <string.h>
#include <dev.h>
#include <memory.h>
#include <fs.h>
#include <elf.h>
#include <block.h>
#include <char.h>
#include <multiboot.h>
#include <mod.h>
#include <cache.h>
#include <cpu.h>
#include <multiboot.h>
#include <rwlock.h>

kernel_symbol_t export_syms[MAX_SYMS];
const char *elf_lookup_symbol (uint32_t addr, elf32_t *elf)
{
	unsigned int i;
	if(elf->lookable != 3) {
		printk(1, "Sorry, can't look up a symbol in this ELF file...\n");
		return 0;
	}
	for (i = 0; i < (elf->symtabsz/sizeof (elf32_symtab_entry_t)); i++)
	{
		if (ELF32_ST_TYPE(elf->symtab[i].info) != 0x2)
			continue;
		if ( (addr >= elf->symtab[i].address) 
				&& (addr < (elf->symtab[i].address + elf->symtab[i].size)) )
		{
			const char *name = (const char *) ((uint32_t)elf->strtab
					+ elf->symtab[i].name);
			return name;
		}
	}
	return 0;
}

#if CONFIG_MODULES

void init_kernel_symbols(void)
{
	uint32_t i;
	for(i = 0; i < MAX_SYMS; i++)
		export_syms[i].ptr = 0;
	/* symbol functions */
	add_kernel_symbol(find_kernel_function);
	add_kernel_symbol(remove_kernel_symbol);
	add_kernel_symbol(_add_kernel_symbol);
	add_kernel_symbol(panic_assert);
	add_kernel_symbol(panic);
	/* basic kernel functions */
	add_kernel_symbol(printk);
	add_kernel_symbol(kprintf);
	add_kernel_symbol(sprintf);
	add_kernel_symbol(memset);
	add_kernel_symbol(memcpy);
	add_kernel_symbol(_strcpy);
	add_kernel_symbol(inb);
	add_kernel_symbol(outb);
	add_kernel_symbol(inw);
	add_kernel_symbol(outw);
	add_kernel_symbol(inl);
	add_kernel_symbol(outl);
	_add_kernel_symbol((unsigned)__super_cli, "__super_cli");
	_add_kernel_symbol((unsigned)__super_sti, "__super_sti");
	add_kernel_symbol(mutex_create);
	add_kernel_symbol(mutex_destroy);
	add_kernel_symbol(__mutex_release);
	add_kernel_symbol(__mutex_acquire);
	add_kernel_symbol(__rwlock_acquire);
	add_kernel_symbol(rwlock_release);
	add_kernel_symbol(__rwlock_escalate);
	add_kernel_symbol(rwlock_create);
	add_kernel_symbol(rwlock_destroy);
	
	/* these systems export these, but have no initialization function */
	add_kernel_symbol(get_epoch_time);
	add_kernel_symbol(allocate_dma_buffer);
}

char *get_symbol_string(uint8_t *buf, uint32_t index)
{  
	uint32_t i;
	char *ret;
	elf_header_t *eh;
	elf32_section_header_t *sh;
	elf32_symtab_entry_t *symtab;
	eh = (elf_header_t *)buf;
	
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf32_section_header_t *)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (elf32_symtab_entry_t *)(buf + sh->offset);
			sh = (elf32_section_header_t *)(buf + eh->shoff + (sh->link * eh->shsize));
			if(sh->type == 3)
			{
				if(!index)
					return (char*)0;
				return (char *)(buf + sh->offset + index);
			}
		}
	}
	return (char *)0;
}

void _add_kernel_symbol(const intptr_t func, const char * funcstr)
{
	uint32_t i;
	if(func < (uint32_t)&kernel_start)
		return;
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(!export_syms[i].ptr)
			break;
	}
	if(i >= MAX_SYMS)
		panic(0, "ran out of space on symbol table");
	export_syms[i].name = funcstr;
	export_syms[i].ptr = func;
}

intptr_t find_kernel_function(char * unres)
{
	uint32_t i;
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(export_syms[i].ptr && 
			strlen(export_syms[i].name) == strlen(unres) &&
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres, 
				(int)strlen(unres)))
			return export_syms[i].ptr;
	}
	return 0;
}

int remove_kernel_symbol(char * unres)
{
	uint32_t i;
	for(i = 0; i < MAX_SYMS; i++)
	{
		if(export_syms[i].ptr && 
			strlen(export_syms[i].name) == strlen(unres) &&
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres, 
				(int)strlen(unres)))
		{
			export_syms[i].ptr=0;
			return 1;
		}
	}
	return 0;
}

elf32_symtab_entry_t * fill_symbol_struct(uint8_t * buf, uint32_t symbol)
{
	uint32_t i;
	elf_header_t * eh;
	elf32_section_header_t * sh;
	elf32_symtab_entry_t * symtab;
	eh = (elf_header_t *)buf;
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf32_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (elf32_symtab_entry_t *)(buf + sh->offset + 
					(symbol * sh->sect_size));
			return (elf32_symtab_entry_t *)symtab;
		}
	}
	return (elf32_symtab_entry_t *)0;
}

intptr_t get_section_offset(uint8_t * buf, uint32_t info)
{
	elf_header_t * eh;
	elf32_section_header_t * sh;
	eh = (elf_header_t*)buf;
	sh = (elf32_section_header_t*)(buf + eh->shoff + (info * eh->shsize));
	return sh->offset;
}
#endif
