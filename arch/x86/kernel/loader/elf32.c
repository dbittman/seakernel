#include <sea/string.h>
#include <sea/dm/dev.h>
#include <sea/mm/vmm.h>
#include <sea/fs/inode.h>
#include <sea/loader/elf.h>
#include <sea/dm/block.h>
#include <sea/dm/char.h>
#include <sea/boot/multiboot.h>
#include <sea/loader/symbol.h>
#include <sea/lib/cache.h>
#include <sea/cpu/processor.h>
#include <sea/boot/multiboot.h>
#include <sea/loader/symbol.h>
#include <sea/fs/file.h>
#include <sea/mm/map.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>

static int process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end)
{
	uint32_t i;
	addr_t entry;
	elf32_header_t *eh = (elf32_header_t *)mem;
	char buffer[(eh->phnum+1)*eh->phsize];
	fs_read_file_data(fp, buffer, eh->phoff, eh->phsize * eh->phnum);
	addr_t max=0, min=~0;
	struct file *file = fs_get_file_pointer(current_process, fp);
	for(i=0;i < eh->phnum;i++)
	{
		elf32_program_header_t *ph = (elf32_program_header_t *)(buffer + (i*eh->phsize));
		
		if((ph->p_addr + ph->p_memsz) > max)
			max = ph->p_addr + ph->p_memsz;
		if(ph->p_addr < min)
			min = ph->p_addr;
		
		if(ph->p_type == PH_LOAD) {
			/* mmap program headers. if the memsz is the same as the filesz, we don't have
			 * to do anything special. if not, then we might need additional mappings: the
			 * file is mapped to some section of the program header's region, and then the
			 * rest is MAP_ANONYMOUS memory. if it fits in the end of the page for the file
			 * mapped memory, then it can fit there. otherwise, we call mmap again.
			 *
			 * also, the actual section we want might be offset from a page. handle that as
			 * well, with inpage_offset.
			 */
			size_t additional = ph->p_memsz - ph->p_filesz;
			size_t inpage_offset = ph->p_addr & (~PAGE_MASK);
			addr_t newend = (ph->p_addr + ph->p_filesz);
			size_t page_free = PAGE_SIZE - (newend % PAGE_SIZE);

			/* calculate the protections for the region. */
			int prot = 0;
			if(ph->p_flags & ELF_PF_R)
				prot |= PROT_READ;
			if(ph->p_flags & ELF_PF_W)
				prot |= PROT_WRITE;
			if(ph->p_flags & ELF_PF_X)
				prot |= PROT_EXEC;

			/* TODO: MAP_SHARED? */
			int flags = MAP_FIXED | MAP_PRIVATE;
			mm_mmap(ph->p_addr & PAGE_MASK, ph->p_filesz + inpage_offset, 
					prot, flags, fp, ph->p_offset & PAGE_MASK);
			if(additional > page_free) {
				mm_mmap((newend & PAGE_MASK) + PAGE_SIZE, additional - page_free,
						prot, flags | MAP_ANONYMOUS, -1, 0);
			}
		}
	}
	fs_fput(current_process, fp, 0);
	if(!max)
		return 0;
	*start = eh->entry;
	*end = (max & PAGE_MASK) + PAGE_SIZE;
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
		total += sh->alignment;
		total += sh->size;
	}
	return total;
}

static void arch_loader_copy_sections(elf32_header_t *header, uint8_t *loaded_buf, struct section_data *sd)
{
	int i;
	elf32_section_header_t *sh;
	addr_t address=(addr_t)loaded_buf;
	sd->symtab = sd->strtab = -1;
	for(i = 0; i < header->shnum; i++)
	{  
		sh = (elf32_section_header_t*)((uint8_t *)header + header->shoff + (i * header->shsize));
		
		if(sh->alignment) {
			address += sh->alignment;
			address &= ~(sh->alignment-1);
		}

		if(sh->type == SHT_NOBITS) {
			memset((void *)address, 0, sh->size);
		} else {
			void *src = (void *)((addr_t)header + sh->offset);
			memcpy((void *)address, src, sh->size);
		}
		
		/* is this the stringtable or the symboltable? */
		elf32_section_header_t *shstr = (elf32_section_header_t*)((uint8_t *)header + header->shoff + (header->strndx * header->shsize));
		if(!strcmp((char *)((uint8_t *)header + shstr->offset + sh->name), ".strtab"))
			sd->strtab = i;
		else if(!strcmp((char *)((uint8_t *)header + shstr->offset + sh->name), ".symtab")) {
			sd->symlen = sh->size;
			sd->symtab = i;
		}

		sd->vbase[i] = address;
		address += sh->size;
	}
	sd->shstrtab = header->strndx;
	sd->num = header->shnum;
}

int arch_loader_relocate_elf_module(void * buf, addr_t *entry, addr_t *tm_exiter, void *load_address, struct section_data *sd)
{
	uint32_t i, x;
	uint32_t module_entry=0, reloc_addr, mem_addr, module_exiter=0;
	elf32_header_t * eh;
	elf32_section_header_t * sh;
	elf32_reloc_entry_t * reloc;
	elf32_symtab_entry_t * symtab;
	int error=0;
	eh = (elf32_header_t *)buf;
	
	arch_loader_copy_sections(eh, load_address, sd);
	
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
					module_entry = sd->vbase[symtab->shndx] + symtab->address;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_exit", 11))
					module_exiter = sd->vbase[symtab->shndx] + symtab->address;
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
				reloc = (elf32_reloc_entry_t*)(sd->vbase[i] + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(reloc->info));
				
				mem_addr = reloc->offset + sd->vbase[sh->info];
				reloc_addr = symtab->address + sd->vbase[symtab->shndx];
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
					reloc_addr += *(intptr_t*)mem_addr;
					reloc_addr -= mem_addr;
					*(intptr_t*)mem_addr = reloc_addr;
				}
				else
				{
					printk(KERN_INFO, "[mod]: invalid relocation type (%x)\n", 
							GET_RELOC_TYPE(reloc->info));
					return 0;
				}
				elf32_symtab_entry_t *ste = &((elf32_symtab_entry_t *)sd->vbase[sd->symtab])[GET_RELOC_SYM(reloc->info)];
				ste->address = mem_addr;
			}
		}
	}
	return 1;
}

const char *arch_loader_lookup_module_symbol(module_t *mq, addr_t addr, char **modname)
{
	/* okay, look up the symbol */
	for (unsigned int i = 0; i < (mq->sd.symlen/sizeof (elf32_symtab_entry_t)); i++)
	{
		elf32_symtab_entry_t *st;
		st = &((elf32_symtab_entry_t *)mq->sd.vbase[mq->sd.symtab])[i];
		if (ELF_ST_TYPE(st->info) != 0x2)
			continue;
		if ( (addr >= st->address) 
				&& (addr < (st->address + st->size)) )
		{
			const char *name = (const char *) ((uint64_t)(mq->sd.vbase[mq->sd.strtab])
					+ st->name);
			*modname = mq->name;
			return name;
		}
	}
	return 0;
}

#endif

