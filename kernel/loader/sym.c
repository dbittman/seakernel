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
		if ( (addr >= elf->symtab[i].address) && (addr < (elf->symtab[i].address + elf->symtab[i].size)) )
		{
			const char *name = (const char *) ((uint32_t)elf->strtab + elf->symtab[i].name);
			return name;
		}
	}
	return 0;
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
	add_kernel_symbol(_strcpy);
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
			symtab = (elf32_symtab_entry_t *)(buf + sh->offset + (symbol * sh->sect_size));
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
