#include <kernel.h>
#include <module.h>
#include <elf.h>
#include <file.h>

int process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end)
{
	uint32_t i, x;
	addr_t entry;
	elf32_header_t *eh = (elf32_header_t *)mem;
	char buffer[(eh->phnum+1)*eh->phsize];
	read_data(fp, buffer, eh->phoff, eh->phsize * eh->phnum);
	addr_t vaddr=0, length=0, offset=0, stop, tmp;
	addr_t max=0, min=~0;
	struct file *file = get_file_pointer((task_t *)current_task, fp);
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
int arch_specific_parse_elf_module(uint8_t * buf, addr_t *entry, addr_t *exiter, addr_t *deps);
int parse_elf_module(module_t *mod, uint8_t * buf, char *name, int force)
{
	int error=0;
	addr_t module_entry=0, module_exiter=0, module_deps=0;

	/* now actually do some error checking... */
	if(!is_valid_elf((char *)buf, 1))
		return _MOD_FAIL;
	
	error = arch_specific_parse_elf_module(buf, &module_entry, &module_exiter, &module_deps);
	
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
