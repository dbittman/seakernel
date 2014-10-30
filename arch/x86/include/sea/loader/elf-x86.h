#ifndef ELF_x86_H
#define ELF_x86_H
#include <sea/loader/module.h>
#include <sea/types.h>
extern void * kernel_start;
#include <sea/loader/elf-x86_common.h>
#include <sea/mm/memory-x86.h>
static inline int is_valid_elf(char *buf, short type)
{
	elf32_header_t * eh;
	eh = (elf32_header_t*)buf;
	if(memcmp(eh->id + 1, (uint8_t*)"ELF", 3)
		|| eh->machine != 0x03
		|| eh->type != type
		|| eh->id[4] != 1 /* 32-bit */
		|| ((eh->entry < EXEC_MINIMUM
		    || eh->entry >= TOP_TASK_MEM_EXEC) && eh->entry))
		return 0;
	return 1;
}

#define GET_RELOC_SYM(i)  ((i)>>8)
#define GET_RELOC_TYPE(i) ((unsigned char)(i))

#define GET_SYMTAB_BIND(i)   ((i)>>4)
#define GET_SYMTAB_TYPE(i)   ((i)&0xf)

#define SHN_UNDEF   0

typedef struct module_s module_t;

int parse_elf_module(module_t *mod, uint8_t * buf, char *name, int);
const char *elf32_lookup_symbol (uint32_t addr, elf32_t *elf);
elf32_t parse_kernel_elf(struct multiboot *mb, elf32_t *);
const char *elf32_lookup_symbol (uint32_t addr, elf32_t *elf);
elf32_symtab_entry_t * fill_symbol_struct(uint8_t * buf, uint32_t symbol);
char *get_symbol_string(uint8_t *buf, uint32_t index);
intptr_t get_section_offset(uint8_t * buf, uint32_t info);

#endif
