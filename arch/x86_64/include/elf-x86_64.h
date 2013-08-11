#ifndef ELF_x86_64_H
#define ELF_x86_64_H
#include <mod.h>
#include <block.h>
#define MAX_SYMS 512
extern void * kernel_start;

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
	/* these aren't....real. */
	char *shbuf;
	unsigned strtab_addr, symtab_addr, strtabsz, syment_len;
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
	sint64_t addend;
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

static inline int is_valid_elf(char *buf, short type)
{
	elf64_header_t * eh;
	eh = (elf64_header_t*)buf;
	if(memcmp(eh->id + 1, (uint8_t*)"ELF", 3)
		|| eh->machine != 62
		|| eh->type != type
		|| eh->id[4] != 2 /* 64 bit */)
		return 0;
	return 1;
}

typedef struct {
	uint64_t d_tag;
	union {
		uint64_t d_val;
		addr_t d_ptr;
	} d_un;
} elf64_dyn_t;

typedef struct
{
	elf64_symtab_entry_t *symtab;
	uint64_t      symtabsz;
	const char   *strtab;
	uint64_t      strtabsz;
	unsigned lookable;
} elf64_t;

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

#include <elf-x86_common.h>

static inline int is_valid_elf32_otherarch(char *buf, short type)
{
	elf32_header_t * eh;
	eh = (elf32_header_t*)buf;
	if(memcmp(eh->id + 1, (uint8_t*)"ELF", 3)
		|| eh->machine != 0x03
		|| eh->type != type
		|| eh->id[4] != 1 /* 32-bit */)
		return 0;
	return 1;
}

int parse_elf_module(module_t *mod, uint8_t * buf, char *name, int);
const char *elf64_lookup_symbol (uint64_t addr, elf64_t *elf);
const char *elf64_lookup_symbol (uint64_t addr, elf64_t *elf);
elf64_symtab_entry_t * fill_symbol_struct(uint8_t * buf, uint64_t symbol);
intptr_t get_section_offset(uint8_t * buf, uint64_t info);
char *get_symbol_string(uint8_t *buf, uint64_t index);

#endif
