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
#include <symbol.h>
#include <cache.h>
#include <cpu.h>
#include <multiboot.h>
#include <rwlock.h>

const char *elf32_lookup_symbol (uint32_t addr, elf32_t *elf)
{
	unsigned int i;
	if(elf->lookable != 3) {
		printk(1, "Sorry, can't look up a symbol in this ELF file...\n");
		return 0;
	}
	for (i = 0; i < (elf->symtabsz/sizeof (elf32_symtab_entry_t)); i++)
	{
		if (ELF_ST_TYPE(elf->symtab[i].info) != 0x2)
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

char *get_symbol_string(uint8_t *buf, uint32_t index)
{  
	uint32_t i;
	char *ret;
	elf32_header_t *eh;
	elf32_section_header_t *sh;
	elf32_symtab_entry_t *symtab;
	eh = (elf32_header_t *)buf;
	
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

elf32_symtab_entry_t * fill_symbol_struct(uint8_t * buf, uint32_t symbol)
{
	uint32_t i;
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	elf32_symtab_entry_t * symtab;
	eh = (elf32_header_t *)buf;
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
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	eh = (elf32_header_t*)buf;
	sh = (elf32_section_header_t*)(buf + eh->shoff + (info * eh->shsize));
	return sh->offset;
}
#endif
