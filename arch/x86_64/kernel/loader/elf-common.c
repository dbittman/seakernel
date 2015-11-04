#include <sea/loader/module.h>
#include <sea/loader/elf.h>
#include <sea/tm/process.h>
#include <sea/fs/file.h>
#include <sea/loader/module.h>
#include <sea/mm/kmalloc.h>

#if (CONFIG_MODULES)

void *arch_loader_parse_elf_module(struct module *mod, uint8_t * buf)
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
