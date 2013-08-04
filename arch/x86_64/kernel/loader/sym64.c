#include <kernel.h>
#include <elf-x86_64.h>

const char *elf64_lookup_symbol (uint64_t addr, elf64_t *elf)
{
	unsigned int i;
	if(elf->lookable != 3) {
		printk(1, "Sorry, can't look up a symbol in this ELF file...\n");
		return 0;
	}
	for (i = 0; i < (elf->symtabsz/sizeof (elf64_symtab_entry_t)); i++)
	{
		if (ELF_ST_TYPE(elf->symtab[i].info) != 0x2)
			continue;
		if ( (addr >= elf->symtab[i].address) 
				&& (addr < (elf->symtab[i].address + elf->symtab[i].size)) )
		{
			const char *name = (const char *) ((uint64_t)elf->strtab
					+ elf->symtab[i].name);
			return name;
		}
	}
	return 0;
}

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
			const char *name = (const char *) ((uint64_t)(elf->strtab)
					+ elf->symtab[i].name);
			return name;
		}
	}
	return 0;
}
#if CONFIG_MODULES

char *get_symbol_string(uint8_t *buf, uint64_t index)
{  
	uint32_t i;
	char *ret;
	elf_header_t *eh;
	elf64_section_header_t *sh;
	elf64_symtab_entry_t *symtab;
	eh = (elf_header_t *)buf;
	
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t *)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (elf64_symtab_entry_t *)(buf + sh->offset);
			sh = (elf64_section_header_t *)(buf + eh->shoff + (sh->link * eh->shsize));
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

elf64_symtab_entry_t * fill_symbol_struct(uint8_t * buf, uint64_t symbol)
{
	uint32_t i;
	elf_header_t * eh;
	elf64_section_header_t * sh;
	elf64_symtab_entry_t * symtab;
	eh = (elf_header_t *)buf;
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (elf64_symtab_entry_t *)(buf + sh->offset + 
					(symbol * sh->sect_size));
			return (elf64_symtab_entry_t *)symtab;
		}
	}
	return (elf64_symtab_entry_t *)0;
}

intptr_t get_section_offset(uint8_t * buf, uint64_t info)
{
	elf_header_t * eh;
	elf64_section_header_t * sh;
	eh = (elf_header_t*)buf;
	sh = (elf64_section_header_t*)(buf + eh->shoff + (info * eh->shsize));
	return sh->offset;
}
#endif
