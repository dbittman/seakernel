#ifndef ELF_x86_64_H
#define ELF_x86_64_H
#include <sea/loader/module.h>
#include <sea/mm/memory-x86_64.h>
#include <sea/string.h>
extern void * kernel_start;
#include <sea/types.h>
typedef struct __attribute__((packed))
{
	uint8_t  id[16];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	addr_t entry;
	uint64_t phoff;
	uint64_t shoff;
	uint32_t flags;
	uint16_t size;
	uint16_t phsize;
	uint16_t phnum;
	uint16_t shsize;
	uint16_t shnum;
	uint16_t strndx;
} elf64_header_t;

typedef elf64_header_t elf_header_t;

typedef struct __attribute__((packed))
{
	uint32_t name;
	uint32_t type;
	uint64_t flags;
	addr_t address;
	uint64_t offset;
	uint64_t size;
	uint32_t link;
	uint32_t info;
	uint64_t alignment;
	uint64_t sect_size;
} elf64_section_header_t;

typedef struct __attribute__((packed)) {
	uint64_t offset;
	uint64_t info;
} elf64_rel_t;

typedef struct __attribute__((packed)) {
	uint64_t offset;
	uint64_t info;
	int64_t addend;
} elf64_rela_t;

typedef struct __attribute__((packed))
{
	uint32_t name;
	uint8_t  info;
	uint8_t  other;
	uint16_t shndx;
	addr_t address;
	uint64_t size;
} elf64_symtab_entry_t;

typedef struct __attribute__((packed))
{
	uint32_t p_type;
	uint32_t p_flags;
	uint64_t p_offset;
	addr_t p_addr;
	addr_t p_paddr;
	uint64_t p_filesz;
	uint64_t p_memsz;
	uint64_t p_align;
} elf64_program_header_t;

static inline int is_valid_elf(unsigned char *buf, short type)
{
	elf64_header_t * eh;
	eh = (elf64_header_t*)buf;
	if(memcmp(eh->id + 1, (uint8_t*)"ELF", 3)
		|| eh->machine != 62
		|| eh->type != type
		|| eh->id[4] != 2 /* 64 bit */
		|| ((eh->entry < MEMMAP_IMAGE_MINIMUM
			|| eh->entry >= MEMMAP_IMAGE_MAXIMUM) && eh->entry))
		return 0;
	return 1;
}

typedef struct  __attribute__((packed)) {
	uint64_t d_tag;
	union {
		uint64_t d_val;
		addr_t d_ptr;
	} d_un;
} elf64_dyn_t;

typedef struct elf
{
	elf64_symtab_entry_t *symtab;
	uint64_t      symtabsz;
	const char   *strtab;
	uint64_t      strtabsz;
	unsigned lookable;
} elf64_t;

const char *elf64_lookup_symbol (uint64_t addr, elf64_t *elf);
const char *elf64_lookup_symbol (uint64_t addr, elf64_t *elf);
elf64_symtab_entry_t *fill_symbol_struct(uint8_t * buf, uint64_t symbol);
intptr_t get_section_offset(uint8_t * buf, uint64_t info);
char *get_symbol_string(uint8_t *buf, uint64_t index);

#endif
