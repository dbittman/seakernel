#include <kernel.h>
#include <string.h>
#include <dev.h>
#include <memory.h>
#include <fs.h>
#include <sea/loader/elf.h>
#include <block.h>
#include <char.h>
#include <sea/boot/multiboot.h>
#include <sea/loader/symbol.h>
#include <cache.h>
#include <cpu.h>
#include <sea/boot/multiboot.h>
#include <sea/loader/symbol.h>
#include <sea/fs/file.h>

static int process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end)
{
	uint32_t i, x;
	addr_t entry;
	elf32_header_t *eh = (elf32_header_t *)mem;
	char buffer[(eh->phnum+1)*eh->phsize];
	fs_read_file_data(fp, buffer, eh->phoff, eh->phsize * eh->phnum);
	addr_t vaddr=0, length=0, offset=0, stop, tmp;
	addr_t max=0, min=~0;
	struct file *file = fs_get_file_pointer((task_t *)current_task, fp);
	for(i=0;i < eh->phnum;i++)
	{
		elf32_program_header_t *ph = (elf32_program_header_t *)(buffer + (i*eh->phsize));
		vaddr = ph->p_addr;
		if(vaddr < min)
			min = vaddr;
		offset = ph->p_offset;
		length = ph->p_filesz;
		stop = vaddr+ph->p_memsz;
		tmp = vaddr&PAGE_MASK;
		if(ph->p_type == PH_LOAD) {
			if(stop > max) max = stop;
			while(tmp <= stop + PAGE_SIZE)
			{
				user_map_if_not_mapped(tmp);
				tmp += PAGE_SIZE;
			}
			if((unsigned)fs_do_sys_read_flags(file, offset, (char *)vaddr, length) != length) {
				fs_fput((task_t *)current_task, fp, 0);
				return 0;
			}
		}
	}
	fs_fput((task_t *)current_task, fp, 0);
	if(!max)
		return 0;
	*start = eh->entry;
	*end = max;
	return 1;
}

int arch_loader_parse_elf_executable(void *mem, int fp, addr_t *start, addr_t *end)
{
	return process_elf32_phdr(mem, fp, start, end);
}

#if (CONFIG_MODULES)

size_t arch_loader_calculate_allocation_size(void *buf)
{
	int i;
	size_t total=0;
	elf32_section_header_t *sh;
	elf32_header_t * header = (elf32_header_t *)buf;
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
		address += sh->size;
	}
	sd->num = header->shnum;
}

int arch_loader_relocate_elf_module(void * buf, addr_t *entry, addr_t *tm_exiter, void *load_address)
{
	uint32_t i, x;
	uint32_t module_entry=0, reloc_addr, mem_addr, module_exiter=0;
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	elf32_reloc_entry_t * reloc;
	elf32_symtab_entry_t * symtab;
	int error=0;
	eh = (elf32_header_t *)buf;
	
	struct section_data sd;
	arch_loader_copy_sections(eh, load_address, &sd);
	
	/* grab the functions we'll need */
	for(i = 0; i < eh->shnum; i++)
	{
		sh = (elf32_section_header_t*)((addr_t)buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				symtab = (elf32_symtab_entry_t*)((addr_t)buf + sh->offset + x);
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_install", 14))
					module_entry = sd.vbase[symtab->shndx] + symtab->address;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_tm_exit", 11))
					module_exiter = sd.vbase[symtab->shndx] + symtab->address;
			}
		}
	}
	
	if(!module_entry)
	{
		printk(KERN_INFO, "[mod]: module_install() entry point was not found\n");
		kfree(load_address);
		return 0;
	}
	
	*entry = module_entry;
	*tm_exiter = module_exiter;
	
	/* fix up the relocation entries */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf32_section_header_t*)((addr_t)buf + eh->shoff + (i * eh->shsize));
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
						return 0;
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
					return 0;
				}
			}
		}
	}
	return 1;
}

#endif
