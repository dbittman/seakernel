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
#include <elf-x86_common.h>
int arch_loader_process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end);
int process_elf(char *mem, int fp, unsigned *start, unsigned *end)
{
	return arch_loader_process_elf32_phdr(mem, fp, start, end);
}

#if (CONFIG_MODULES)

#define MAX_SECTIONS 32

struct section_data {
	addr_t vbase[MAX_SECTIONS];
	elf32_section_header_t *sects[MAX_SECTIONS];
	int num;
};

static size_t arch_loader_calculate_allocation_size(elf32_header_t *header)
{
	int i;
	size_t total=0;
	elf32_section_header_t *sh;
	for(i = 0; i < header->shnum; i++)
	{  
		sh = (elf32_section_header_t*)((uint8_t *)header + header->shoff + (i * header->shsize));
		total += sh->size;
	}
	return total;
}

static void arch_loader_copy_sections(elf32_header_t *header, uint8_t *loaded_buf, struct section_data *sd)
{
	int i;
	elf32_section_header_t *sh;
	addr_t address=(addr_t)loaded_buf;
	for(i = 0; i < header->shnum; i++)
	{  
		sh = (elf32_section_header_t*)((uint8_t *)header + header->shoff + (i * header->shsize));
		
		if(sh->type == SHT_NOBITS) {
			memset((void *)address, 0, sh->size);
		} else {
			void *src = (void *)((addr_t)header + sh->offset);
			memcpy((void *)address, src, sh->size);
		}
		sd->vbase[i] = address;
		sd->sects[i] = sh;
		address += sh->size;
	}
	sd->num = header->shnum;
}

int arch_loader_loader_parse_elf_module(uint8_t * buf, addr_t *entry, addr_t *tm_exiter, addr_t *deps, void **loaded_location)
{
	uint32_t i, x;
	uint32_t module_entry=0, reloc_addr, mem_addr, module_exiter=0, module_deps=0;
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	elf32_reloc_entry_t * reloc;
	elf32_symtab_entry_t * symtab;
	int error=0;
	eh = (elf32_header_t *)buf;
	
	
	void *load_address;
	size_t load_size;
	struct section_data sd;
	
	load_size = arch_loader_calculate_allocation_size(eh);
	load_address = kmalloc(load_size);
	arch_loader_copy_sections(eh, load_address, &sd);
	
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
					module_entry = sd.vbase[symtab->shndx] + symtab->address;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_tm_exit", 11))
					module_exiter = sd.vbase[symtab->shndx] + symtab->address;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_deps", 11))
					module_deps = sd.vbase[symtab->shndx] + symtab->address;
			}
		}
	}
	
	if(!module_entry)
	{
		printk(KERN_INFO, "[mod]: module_install() entry point was not found\n");
		kfree(load_address);
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
				reloc = (elf32_reloc_entry_t*)(sd.vbase[i] + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(reloc->info));
				
				mem_addr = reloc->offset + sd.vbase[sh->info];
				reloc_addr = symtab->address + sd.vbase[symtab->shndx];
				/* external reference (kernel symbol most likely) */
				if(symtab->shndx == 0)
				{
					reloc_addr = loader_find_kernel_function(get_symbol_string(buf, symtab->name));
					if(!reloc_addr)
					{
						printk(KERN_INFO, "[mod]: unresolved dependency \"%s\"\n", 
								get_symbol_string(buf, symtab->name));
						error++;
					}
				}
				
				/* absolute physical address */
				if(GET_RELOC_TYPE(reloc->info) == 0x01)
				{
					reloc_addr += *(intptr_t*)mem_addr;
					*(intptr_t*)mem_addr = reloc_addr;
				}
				/* relative physical address */
				else if(GET_RELOC_TYPE(reloc->info) == 0x02)
				{
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
	if(error)
		kfree(load_address);
	else
		*loaded_location = load_address;
	return error;
}

#endif
