
#include <kernel.h>
#include <sea/loader/elf.h>

int loader_parse_elf_executable(void *mem, int fp, addr_t *start, addr_t *end)
{
	return arch_loader_parse_elf_executable(mem, fp, start, end);
}

void *loader_parse_elf_module(module_t *mod, void * buf)
{
	return arch_loader_parse_elf_module(mod, buf);
}
