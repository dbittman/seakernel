#ifndef __ELF_H
#define __ELF_H
#include <sea/boot/multiboot.h>
#include <sea/arch-include/loader-elf.h>

void *loader_parse_elf_module(module_t *mod, void * buf);
int arch_loader_parse_elf_executable(void *mem, int fp, addr_t *start, addr_t *end);
int loader_parse_elf_executable(void *mem, int fp, addr_t *start, addr_t *end);

void *arch_loader_parse_elf_module(module_t *mod, uint8_t * buf);
size_t arch_loader_calculate_allocation_size(void *buf);
int arch_loader_relocate_elf_module(void * buf, addr_t *entry, addr_t *tm_exiter, void *load_address);

void loader_parse_kernel_elf(struct multiboot *mb, void *elf);
void arch_loader_parse_kernel_elf(struct multiboot *mb, void *elf);

#endif
