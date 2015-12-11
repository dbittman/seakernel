#ifndef __ELF_H
#define __ELF_H
#include <sea/boot/multiboot.h>
#include <sea/arch-include/loader-elf.h>
#include <sea/loader/symbol.h>
#define GET_RELOC_SYM(i)  ((i)>>32)
#define GET_RELOC_TYPE(i) (i & 0xFFFFFFFF)

#define GET_SYMTAB_BIND(i)   ((i)>>4)
#define GET_SYMTAB_TYPE(i)   ((i)&0xf)

#define SHN_UNDEF   0

#define R_X86_64_NONE		0	/* No reloc */
#define R_X86_64_64			1	/* Direct 64 bit  */
#define R_X86_64_PC32		2	/* PC relative 32 bit signed */
#define R_X86_64_GOT32		3	/* 32 bit GOT entry */
#define R_X86_64_PLT32		4	/* 32 bit PLT address */
#define R_X86_64_COPY		5	/* Copy symbol at runtime */
#define R_X86_64_RELATIVE	8	/* Adjust by program base */
#define R_X86_64_32			10	/* Direct 32 bit zero extended */
#define R_X86_64_32S		11	/* Direct 32 bit sign extended */
#define R_X86_64_16			12	/* Direct 16 bit zero extended */
#define R_X86_64_PC16		13	/* 16 bit sign extended pc relative */

#define PH_LOAD    1
#define PH_DYNAMIC 2
#define PH_INTERP  3

#define SHT_NOBITS 8

#define EDT_NEEDED   1
#define EDT_PLTRELSZ 2
#define EDT_STRTAB   5
#define EDT_SYMTAB   6

#define EDT_STRSZ   10
#define EDT_SYMENT  11

#define EDT_REL     17
#define EDT_RELSZ   18
#define EDT_RELENT  19
#define EDT_PLTREL  20

#define EDT_TEXTREL 22
#define EDT_JMPREL  23

#define ELF_PF_R 4
#define ELF_PF_W 2
#define ELF_PF_X 1

#define ELF_ST_TYPE_FUNCTION 2
#define ELF_ST_TYPE(i) ((i)&0xf)

struct module;
struct file;

void *loader_parse_elf_module(struct module *mod, void * buf);
int arch_loader_parse_elf_executable(void *mem, struct file *file, addr_t *start, addr_t *end);
int loader_parse_elf_executable(void *mem, struct file *, addr_t *start, addr_t *end);

void *arch_loader_parse_elf_module(struct module *mod, uint8_t * buf);
size_t arch_loader_calculate_allocation_size(void *buf);
int arch_loader_relocate_elf_module(void * buf, addr_t *entry, addr_t *tm_exiter, void *load_address, struct section_data *sd);

void loader_parse_kernel_elf(struct multiboot *mb, struct section_data *);
void arch_loader_parse_kernel_elf(struct multiboot *mb, struct section_data *);

#endif
