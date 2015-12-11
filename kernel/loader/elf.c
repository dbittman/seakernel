#include <sea/loader/elf.h>
#include <sea/boot/multiboot.h>
#include <sea/fs/file.h>

int loader_parse_elf_executable(void *mem, struct file *file, addr_t *start, addr_t *end)
{
	return arch_loader_parse_elf_executable(mem, file, start, end);
}

void *loader_parse_elf_module(struct module *mod, void * buf)
{
	return arch_loader_parse_elf_module(mod, buf);
}

void loader_parse_kernel_elf(struct multiboot *mb, struct section_data *sd)
{
	arch_loader_parse_kernel_elf(mb, sd);
}
