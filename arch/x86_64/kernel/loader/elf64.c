#include <kernel.h>
#include <mod.h>
#include <elf-x86_64.h>

int process_elf64_phdr(char *mem, int fp, addr_t *start, addr_t *end)
{
	uint32_t i, x;
	addr_t entry;
	elf_header_t *eh = (elf_header_t *)mem;
	char buffer[(eh->phnum+1)*eh->phsize];
	read_data(fp, buffer, eh->phoff, eh->phsize * eh->phnum);
	uint64_t vaddr=0, length=0, offset=0, stop, tmp;
	uint64_t max=0, min=~0;
	struct file *file = get_file_pointer((task_t *)current_task, fp);
	for(i=0;i < eh->phnum;i++)
	{
		elf64_program_header_t *ph = (elf64_program_header_t *)(buffer + (i*eh->phsize));
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
			if((unsigned)do_sys_read_flags(file, offset, (char *)vaddr, length) != length) {
				fput((task_t *)current_task, fp, 0);
				return 0;
			}
		}
	}
	fput((task_t *)current_task, fp, 0);
	if(!max)
		return 0;
	*start = eh->entry;
	*end = max;
	return 1;
}

int process_elf(char *mem, int fp, addr_t *start, addr_t *end)
{
	return process_elf64_phdr(mem, fp, start, end);
}

elf32_t parse_kernel_elf(struct multiboot *mb, elf32_t *elf)
{
	unsigned int i;
	elf32_section_header_t *sh = (elf32_section_header_t*)(addr_t)mb->addr;
	elf->lookable=0;
	uint32_t shstrtab = sh[mb->shndx].address;
	for (i = 0; i < (unsigned)mb->num; i++)
	{
		const char *name = (const char *) ((addr_t)shstrtab + sh[i].name);
		if (!strcmp (name, ".strtab"))
		{
			elf->lookable |= 1;
			elf->strtab = (const char *)(addr_t)sh[i].address;
			elf->strtabsz = sh[i].size;
		}
		if (!strcmp (name, ".symtab"))
		{
			elf->lookable |= 2;
			elf->symtab = (elf32_symtab_entry_t *)(addr_t)sh[i].address;
			elf->symtabsz = sh[i].size;
		}
	}
	return *elf;
}

#if (CONFIG_MODULES)

void elf64_write_field(int type, addr_t mem_addr, addr_t reloc_addr)
{
	switch(type)
	{
		case R_X86_64_32: case R_X86_64_PC32:
			*(uint32_t*)mem_addr = (uint32_t)reloc_addr;
			break;
		default:
			*(uint64_t*)mem_addr = reloc_addr;
			break;
	}
}

int parse_elf_module(module_t *mod, uint8_t * buf, char *name, int force)
{
	uint32_t i, x;
	uint64_t module_entry=0, reloc_addr, mem_addr, module_exiter=0, module_deps=0;
	elf_header_t * eh;
	elf64_section_header_t * sh;
	elf64_rel_t * reloc;
	elf64_rela_t * rela;
	elf64_symtab_entry_t * symtab;
	int error=0;
	eh = (elf_header_t *)buf;
	
	/* now actually do some error checking... */
	if(!is_valid_elf((char *)buf, 1))
		return _MOD_FAIL;
	/* grab the functions we'll need */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				symtab = (elf64_symtab_entry_t*)(buf + sh->offset + x);
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_install", 14))
					module_entry = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_exit", 11))
					module_exiter = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), 
						(uint8_t*)"module_deps", 11))
					module_deps = get_section_offset(buf, symtab->shndx) + 
						symtab->address + (uint64_t)buf;
			}
		}
	}
	
	if(!module_entry)
	{
		printk(KERN_INFO, "[mod]: module_install() entry point was not found\n");
		return _MOD_FAIL;
	}
	
	/* fix up the relocation entries */
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (elf64_section_header_t*)(buf + eh->shoff + (i * eh->shsize));
		/* 64-bit ELF only deals in rela relocation sections */
		if(sh->type == 4) {
			
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				rela = (elf64_rela_t*)(buf + sh->offset + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(rela->info));
				mem_addr = (uint64_t)buf + rela->offset;
				mem_addr += get_section_offset(buf, sh->info);
				
				reloc_addr = (uint64_t)buf + symtab->address;
				reloc_addr += get_section_offset(buf, symtab->shndx);
				if(symtab->shndx == 0)
				{
					reloc_addr = find_kernel_function(get_symbol_string(buf, symtab->name));
					if(!reloc_addr)
					{
						printk(KERN_INFO, "[mod]: %x: unresolved dependency \"%s\"\n", 
								rela->info, get_symbol_string(buf, symtab->name));
						error++;
					}
				} else {
					if(GET_RELOC_TYPE(rela->info) == R_X86_64_64)
					{
						reloc_addr += *(uint64_t *)(mem_addr) + rela->addend;
					}
					else if(GET_RELOC_TYPE(rela->info) == R_X86_64_32) 
					{
						reloc_addr += *(uint64_t *)(mem_addr) + rela->addend;
					}
					else
					{
						printk(KERN_INFO, "[mod]: invalid relocation type (%x)\n", 
								GET_RELOC_TYPE(rela->info));
						error++;
					}
				}
				elf64_write_field(GET_RELOC_TYPE(rela->info), mem_addr, reloc_addr);
				
			}	
			
		}
	}
	
	if(module_deps)
	{
		/* Load more deps */
		char deps_str[128];
		memset(deps_str, 0, 128);
		unsigned kver = ((int (*)(char *))module_deps)(deps_str);
		if(kver != KVERSION && !force)
		{
			printk(3, "[mod]: Module '%s' was compiled for a different kernel version!\n", 
					mod->name);
			return _MOD_FAIL;
		}
		strncpy(mod->deps, deps_str, 128);
		/* make sure all deps are loaded */
		char *cur = deps_str;
		while(1) {
			char *n = strchr(cur, ',');
			if(!n) break;
			*n=0;
			n++;
			if(!is_loaded(cur)) {
				printk(3, "[mod]: Module '%s' has missing dependency '%s'\n", mod->name, cur);
				return _MOD_FAIL;
			}
			cur = n;
		}
	}
	if(error)
		return _MOD_FAIL;
	mod->entry=module_entry;
	mod->exiter=module_exiter;
	return _MOD_GO;
}
#endif
