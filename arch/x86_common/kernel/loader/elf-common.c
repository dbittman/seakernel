#include <sea/loader/module.h>
#include <sea/loader/elf.h>
#include <sea/tm/process.h>
#include <sea/fs/file.h>
#include <sea/loader/module.h>
#include <sea/tm/schedule.h>
#include <sea/mm/kmalloc.h>

void arch_loader_parse_kernel_elf(struct multiboot *mb, void *__elf)
{
	unsigned int i;
	elf32_t *elf = __elf;
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
}

#if (CONFIG_MODULES)

void *arch_loader_parse_elf_module(module_t *mod, uint8_t * buf)
{
	int error=0;
	addr_t module_entry=0, module_exiter=0;

	/* now actually do some error checking... */
	if(!is_valid_elf((char *)buf, 1))
		return 0;
	size_t load_size = arch_loader_calculate_allocation_size(buf);
	void *load = kmalloc(load_size);
	if(!arch_loader_relocate_elf_module(buf, &module_entry, &module_exiter, load, &mod->sd))
	{
		kfree(load);
		return 0;
	}
	mod->base = load;
	mod->length = load_size;
	mod->entry=module_entry;
	mod->exiter=module_exiter;
	return load;
}

#endif
