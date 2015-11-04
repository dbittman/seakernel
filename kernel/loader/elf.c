#include <sea/loader/elf.h>
#include <sea/boot/multiboot.h>
#include <sea/loader/elf.h>

int loader_parse_elf_executable(void *mem, int fp, addr_t *start, addr_t *end)
{
	return arch_loader_parse_elf_executable(mem, fp, start, end);
}

void *loader_parse_elf_module(struct module *mod, void * buf)
{
	return arch_loader_parse_elf_module(mod, buf);
}

void loader_parse_kernel_elf(struct multiboot *mb, void *elf)
{
	arch_loader_parse_kernel_elf(mb, elf);
}
