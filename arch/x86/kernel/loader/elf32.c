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
#include <symbol.h>
#include <file.h>
int arch_loader_process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end);
int process_elf(char *mem, int fp, unsigned *start, unsigned *end)
{
	return arch_loader_process_elf32_phdr(mem, fp, start, end);
}

#if (CONFIG_MODULES)

static size_t arch_loader_calculate_allocation_size(elf32_header_t *header)
{
	int i, x;
	size_t total=0;
	elf32_section_header_t *sh;
	for(i = 0; i < header->shnum; i++)
	{  
		sh = (elf32_section_header_t*)((uint8_t *)header + header->shoff + (i * header->shsize));
		total += sh->sect_size;
	}
	return total;
}

static void arch_loader_copy_sections(elf32_header_t *header, uint8_t *loaded_buf)
{
	
}

int arch_loader_loader_parse_elf_module(uint8_t * buf, addr_t *entry, addr_t *tm_exiter, addr_t *deps)
{
	uint32_t i, x;
	uint32_t module_entry=0, reloc_addr, mem_addr, module_exiter=0, module_deps=0;
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	elf32_reloc_entry_t * reloc;
	elf32_symtab_entry_t * symtab;
	int error=0;
	eh = (elf32_header_t *)buf;
	
	/* grab the functions we'll need */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf32_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				symtab = (elf32_symtab_entry_t*)(buf + sh->offset + x);
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_install", 14))
					module_entry = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint32_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_tm_exit", 11))
					module_exiter = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint32_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_deps", 11))
					module_deps = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint32_t)buf;
			}
		}
	}
	
	if(!module_entry)
	{
		printk(KERN_INFO, "[mod]: module_install() entry point was not found\n");
		return 1;
	}
	
	*entry = module_entry;
	*tm_exiter = module_exiter;
	*deps = module_deps;
	
	/* fix up the relocation entries */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf32_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 9)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				reloc = (elf32_reloc_entry_t*)(buf + sh->offset + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(reloc->info));
				
				/* absolute physical address */
				if(GET_RELOC_TYPE(reloc->info) == 0x01)
				{
					mem_addr = (uint32_t)buf + reloc->offset;
					mem_addr += get_section_offset(buf, sh->info);
					/* external reference (kernel symbol most likely) */
					if(symtab->shndx == 0)
					{
						reloc_addr = loader_find_kernel_function(get_symbol_string(buf, symtab->name));
						if(!reloc_addr)
						{
							printk(KERN_INFO, "[mod]: *ABS* unresolved dependency \"%s\"\n", 
									get_symbol_string(buf, symtab->name));
							error++;
						}
					}
					else
					{
						reloc_addr = (uint32_t)buf + symtab->address + *(intptr_t*)mem_addr;
						reloc_addr += get_section_offset(buf, symtab->shndx);
					}
					*(intptr_t*)mem_addr = reloc_addr;
				}
				/* relative physical address */
				else if(GET_RELOC_TYPE(reloc->info) == 0x02)
				{
					mem_addr = (uint32_t)buf + reloc->offset;
					mem_addr += get_section_offset(buf, sh->info);
					/* external reference (kernel symbol most likely) */
					if(symtab->shndx == 0)
					{
						reloc_addr = loader_find_kernel_function(get_symbol_string(buf, 
								symtab->name));
						if(!reloc_addr)
						{
							printk(KERN_INFO, "[mod]: *REL* unresolved dependency \"%s\"\n", 
									get_symbol_string(buf, symtab->name));
							error++;
						}
					}
					else
					{
						reloc_addr = (uint32_t)buf + symtab->address;
						reloc_addr += get_section_offset(buf, symtab->shndx);
					}
					/* we need to make this relative to the memory address */
					reloc_addr = mem_addr - reloc_addr + 4;
					*(intptr_t*)mem_addr = -reloc_addr;
				}
				else
				{
					printk(KERN_INFO, "[mod]: invalid relocation type (%x)\n", 
							GET_RELOC_TYPE(reloc->info));
					error++;
				}
			}
		}
	}
	
	return error;
}
#endif
