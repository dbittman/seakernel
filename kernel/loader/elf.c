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
#define MAX_SYMS 512
extern int load_deps(char *);
int load_deps_c(char *d);
struct { 
	const char *name; 
	intptr_t ptr; 
	int flag;
} export_syms[MAX_SYMS];
#include <multiboot.h>
#define ELF32_ST_TYPE(i) ((i)&0xf)
char * get_symbol_string(uint8_t * buf, uint32_t index)
{  
	uint32_t i;
	char * ret;
	struct ELF_header_s * eh;
	struct ELF_section_header_s * sh;
	struct ELF_symtab_entry_s * symtab;
	
	eh = (struct ELF_header_s*)buf;
	
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (struct ELF_section_header_s*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (struct ELF_symtab_entry_s*)(buf + sh->offset);
			sh = (struct ELF_section_header_s*)(buf + eh->shoff + (sh->link * eh->shsize));
			if(sh->type == 3)
			{
				if(!index)
					return (char*)" ";
				return (char *)(buf + sh->offset + index);
			}
		}
	}
	
	ret = (char*)0;
	return ret;
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
		return;
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
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres, (int)strlen(unres)))
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
			!memcmp((uint8_t*)export_syms[i].name, (uint8_t*)unres, (int)strlen(unres)))
		{
			export_syms[i].ptr=0;
			return 1;
		}
	}
	return 0;
}

struct ELF_symtab_entry_s * fill_symbol_struct(uint8_t * buf, uint32_t symbol)
{
	uint32_t i;
	struct ELF_header_s * eh;
	struct ELF_section_header_s * sh;
	struct ELF_symtab_entry_s * symtab;
	eh = (struct ELF_header_s*)buf;
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (struct ELF_section_header_s*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			symtab = (struct ELF_symtab_entry_s*)(buf + sh->offset + (symbol * sh->sect_size));
			return (struct ELF_symtab_entry_s *)symtab;
		}
	}
	return (struct ELF_symtab_entry_s *)0;
}

intptr_t get_section_offset(uint8_t * buf, uint32_t info)
{
	struct ELF_header_s * eh;
	struct ELF_section_header_s * sh;
	eh = (struct ELF_header_s*)buf;
	sh = (struct ELF_section_header_s*)(buf + eh->shoff + (info * eh->shsize));
	return sh->offset;
}

void init_kernel_symbols(void)
{
	uint32_t i;
	for(i = 0; i < MAX_SYMS; i++)
		export_syms[i].ptr = 0;
	add_kernel_symbol(_add_kernel_symbol);
	add_kernel_symbol(do_iremove);
	add_kernel_symbol(write_block_cache);
	add_kernel_symbol(printk);
	add_kernel_symbol(kprintf);
	add_kernel_symbol(sprintf);
	add_kernel_symbol(inb);
	add_kernel_symbol(pfs_cn_node);
	add_kernel_symbol(pfs_cn);
	add_kernel_symbol(outb);
	add_kernel_symbol(inw);
	add_kernel_symbol(outw);
	add_kernel_symbol(inl);
	add_kernel_symbol(ttyx_ioctl);
	add_kernel_symbol(outl);
	//add_kernel_symbol(set_console_font);
	add_kernel_symbol(vm_unmap_only);
	add_kernel_symbol(delay);
	add_kernel_symbol(delay_sleep);
	add_kernel_symbol(memset);
	add_kernel_symbol(memcpy);
	add_kernel_symbol(strcpy);
	add_kernel_symbol(proc_set_callback);
	add_kernel_symbol(proc_get_major);
	add_kernel_symbol(panic);
	add_kernel_symbol(schedule);
	add_kernel_symbol(force_schedule);
	add_kernel_symbol(run_scheduler);
	add_kernel_symbol(do_get_idir);
	add_kernel_symbol(register_interrupt_handler);
	add_kernel_symbol(unregister_interrupt_handler);
	add_kernel_symbol(get_interrupt_handler);
	add_kernel_symbol(serial_puts);
	add_kernel_symbol(set_blockdevice);
	add_kernel_symbol(set_chardevice);
	add_kernel_symbol(exit);
	add_kernel_symbol(set_availablebd);
	add_kernel_symbol(set_availablecd);
	add_kernel_symbol(unregister_block_device);
	add_kernel_symbol(unregister_char_device);
	add_kernel_symbol(dfs_cn);
	add_kernel_symbol(remove_dfs_node);
	add_kernel_symbol(block_read);
	add_kernel_symbol(do_block_rw);
	add_kernel_symbol(block_write);
	add_kernel_symbol(sys_setsid);
	//add_kernel_symbol(setup_console_video);
	add_kernel_symbol(__kmalloc);
	add_kernel_symbol(kmalloc_ap);
	add_kernel_symbol(kmalloc_a);
	add_kernel_symbol(kmalloc_p);
	add_kernel_symbol(kfree);
	add_kernel_symbol(dosyscall);
	add_kernel_symbol(vm_map);
	add_kernel_symbol(vm_unmap);
	add_kernel_symbol(init_console);
	add_kernel_symbol(create_console);
	add_kernel_symbol(destroy_console);
	//add_kernel_symbol(set_console_callbacks);
	add_kernel_symbol(fork);
	add_kernel_symbol(kill_task);
	add_kernel_symbol(__wait_flag);
	add_kernel_symbol(wait_flag_except);
	_add_kernel_symbol((unsigned)(unsigned *)&curcons, "curcons");
	_add_kernel_symbol((unsigned)(char *)&tables, "tables");
#ifndef CONFIG_SMP
	_add_kernel_symbol((unsigned)(task_t **)&current_task, "current_task");
#else
	add_kernel_symbol(get_cpu);
#endif
	_add_kernel_symbol((unsigned)(task_t **)&kernel_task, "kernel_task");
	_add_kernel_symbol((unsigned)(cpu_t *)&primary_cpu, "primary_cpu");
	_add_kernel_symbol((unsigned)(struct inode **)&kproclist, "kproclist");
	add_kernel_symbol(sys_open);
	add_kernel_symbol(sys_read);
	add_kernel_symbol(set_as_kernel_task);
	add_kernel_symbol(sys_write);
	add_kernel_symbol(sys_close);
	add_kernel_symbol(read_fs);
	add_kernel_symbol(write_fs);
	add_kernel_symbol(sys_ioctl);
	add_kernel_symbol(proc_append_buffer);
	add_kernel_symbol(sys_stat);
	add_kernel_symbol(sys_fstat);
	_add_kernel_symbol((unsigned)__super_cli, "__super_cli");
	_add_kernel_symbol((unsigned)__super_sti, "__super_sti");
	add_kernel_symbol(get_epoch_time);
	add_kernel_symbol(disconnect_block_cache);
	add_kernel_symbol(register_sbt);
	add_kernel_symbol(unregister_sbt);
	add_kernel_symbol(block_rw);
	add_kernel_symbol(get_empty_cache);
	add_kernel_symbol(find_cache_element);
	add_kernel_symbol(do_cache_object);
	add_kernel_symbol(remove_element);
	add_kernel_symbol(sync_element);
	add_kernel_symbol(destroy_cache);
	add_kernel_symbol(sync_cache);
	
	add_kernel_symbol(create_mutex);
	add_kernel_symbol(__destroy_mutex);
	add_kernel_symbol(__mutex_on);
	add_kernel_symbol(__mutex_off);
	add_kernel_symbol(block_ioctl);
	add_kernel_symbol(block_device_rw);
	add_kernel_symbol(iput);
	add_kernel_symbol(find_kernel_function);
	add_kernel_symbol(remove_kernel_symbol);
	add_kernel_symbol(do_send_signal);
	add_kernel_symbol(switch_console);
}

elf_t parse_kernel_elf(struct multiboot *mb, elf_t *elf)
{
	unsigned int i;
	struct ELF_section_header_s *sh = (struct ELF_section_header_s*)mb->addr;
	elf->lookable=0;
	uint32_t shstrtab = sh[mb->shndx].address;
	for (i = 0; i < (unsigned)mb->num; i++)
	{
		const char *name = (const char *) (shstrtab + sh[i].name);
		if (!strcmp (name, ".strtab"))
		{
			elf->lookable |= 1;
			elf->strtab = (const char *)sh[i].address;
			elf->strtabsz = sh[i].size;
		}
		if (!strcmp (name, ".symtab"))
		{
			elf->lookable |= 2;
			elf->symtab = (struct ELF_symtab_entry_s *)sh[i].address;
			elf->symtabsz = sh[i].size;
		}
	}
	return *elf;
}

const char *elf_lookup_symbol (uint32_t addr, elf_t *elf)
{
	unsigned int i;
	if(elf->lookable != 3) {
		printk(1, "Sorry, can't look up a symbol in this ELF file...\n");
		return 0;
	}
	for (i = 0; i < (elf->symtabsz/sizeof (struct ELF_symtab_entry_s)); i++)
	{
		if (ELF32_ST_TYPE(elf->symtab[i].info) != 0x2)
			continue;
		if ( (addr >= elf->symtab[i].address) && (addr < (elf->symtab[i].address + elf->symtab[i].size)) )
		{
			const char *name = (const char *) ((uint32_t)elf->strtab + elf->symtab[i].name);
			return name;
		}
	}
	return 0;
}

int parse_elf_module(module_t *mod, uint8_t * buf, char *name)
{
	uint32_t i, x;
	uint32_t module_entry, reloc_addr, mem_addr, module_exiter=0, module_deps=0;
	struct ELF_header_s * eh;
	struct ELF_section_header_s * sh;
	struct ELF_reloc_entry_s * reloc;
	struct ELF_symtab_entry_s * symtab;
	
	eh = (struct ELF_header_s*)buf;
	
	/* now actually do some error checking... */
	if(memcmp(eh->id + 1, (uint8_t*)"ELF", 3) || eh->type != 0x01 || eh->machine != 0x03 || eh->entry != 0x00)
	{
		printk(KERN_INFO, "[mod]: Error: invalid ELF file\n"); 
		return _MOD_FAIL;
	}
	int error=0;
	/* do section header stuff here */
	module_entry = 0;
	for(i = 0; i < eh->shnum; i++)
	{  
		sh = (struct ELF_section_header_s*)(buf + eh->shoff + (i * eh->shsize));
		if(sh->type == 2)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				symtab = (struct ELF_symtab_entry_s*)(buf + sh->offset + x);
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), (uint8_t*)"module_install", 14))
					module_entry = get_section_offset(buf, symtab->shndx) + symtab->address + (uint32_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), (uint8_t*)"module_exit", 11))
					module_exiter = get_section_offset(buf, symtab->shndx) + symtab->address + (uint32_t)buf;
				if(!memcmp((uint8_t*)get_symbol_string(buf, symtab->name), (uint8_t*)"module_deps", 11))
					module_deps = get_section_offset(buf, symtab->shndx) + symtab->address + (uint32_t)buf;
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
		sh = (struct ELF_section_header_s*)(buf + eh->shoff + (i * eh->shsize));
		
		if(sh->type == 9)
		{
			for(x = 0; x < sh->size; x += sh->sect_size)
			{
				reloc = (struct ELF_reloc_entry_s*)(buf + sh->offset + x);
				symtab = fill_symbol_struct(buf, GET_RELOC_SYM(reloc->info));
				
				/* absolute physical address */
				if(GET_RELOC_TYPE(reloc->info) == 0x01)
				{
					mem_addr = (uint32_t)buf + reloc->offset;
					mem_addr += get_section_offset(buf, sh->info);
					
					/* external reference (kernel symbol most likely) */
					if(symtab->shndx == 0)
					{
						reloc_addr = find_kernel_function(get_symbol_string(buf, symtab->name));
						if(!reloc_addr)
						{
							printk(KERN_INFO, "[mod]: *ABS* unresolved dependency \"%s\"\n", get_symbol_string(buf, symtab->name));
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
						reloc_addr = find_kernel_function(get_symbol_string(buf, symtab->name));
						
						if(!reloc_addr)
						{
							printk(KERN_INFO, "[mod]: *REL* unresolved dependency \"%s\"\n", get_symbol_string(buf, symtab->name));
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
					printk(KERN_INFO, "[mod]: invalid relocation type (%x)\n", GET_RELOC_TYPE(reloc->info));
					error++;
				}
			}
		}
	}
	
	if(module_deps)
	{
		/* Load more deps */
		char deps_str[128];
		memset(deps_str, 0, 128);
		unsigned kver = ((int (*)(char *))module_deps)(deps_str);
		if(kver != KVERSION)
		{
			kprintf("[mod]: Module '%s' was compiled for a different kernel version!\n", mod->name);
			return _MOD_FAIL;
		}
		strcpy(mod->deps, deps_str);
		int dl=0;
		if((dl=load_deps_c(deps_str)) == -1)
			return _MOD_FAIL;
		if(!dl && error)
			return _MOD_FAIL;
		if(error || dl) {
			printk(1, "[mod]: Trying again...\n");
			return _MOD_AGAIN;
		}
	} else if(error)
		return _MOD_FAIL;
	mod->entry=module_entry;
	mod->exiter=module_exiter;
	return _MOD_GO;
}
