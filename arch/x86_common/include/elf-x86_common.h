#ifndef __ELF_X86_COMMON_H
#define __ELF_X86_COMMON_H
#include <types.h>
#include <multiboot.h>
typedef struct __attribute__((packed))
{
	uint8_t  id[16];
	uint16_t type;
	uint16_t machine;
	uint32_t version;
	uint32_t entry;
	uint32_t phoff;
	uint32_t shoff;
	uint32_t flags;
	uint16_t size;
	uint16_t phsize;
	uint16_t phnum;
	uint16_t shsize;
	uint16_t shnum;
	uint16_t strndx;
	char *shbuf;
	unsigned strtab_addr, symtab_addr, strtabsz, syment_len;
} elf32_header_t;

typedef struct __attribute__((packed))
{
	uint32_t name;
	uint32_t type;
	uint32_t flags;
	uint32_t address;
	uint32_t offset;
	uint32_t size;
	uint32_t link;
	uint32_t info;
	uint32_t alignment;
	uint32_t sect_size;
} elf32_section_header_t;

typedef struct __attribute__((packed))
{
	int   offset;
	uint32_t  info;
} elf32_reloc_entry_t;

typedef struct __attribute__((packed)) {
	uint32_t r_offset;
	uint32_t r_info;
} elf32_rel_t;

typedef struct __attribute__((packed))
{
	uint32_t name;
	uint32_t address;
	uint32_t size;
	uint8_t  info;
	uint8_t  other;
	uint16_t shndx;
} elf32_symtab_entry_t;

typedef struct __attribute__((packed))
{
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_addr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} elf32_program_header_t;

typedef struct {
	uint16_t d_tag;
	union {
		uint32_t d_val;
		uint32_t d_ptr;
	} d_un;
} elf32_dyn_t;

typedef struct
{
	elf32_symtab_entry_t *symtab;
	uint32_t      symtabsz;
	const char   *strtab;
	uint32_t      strtabsz;
	unsigned lookable;
} elf32_t;

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

#define ELF_ST_TYPE(i) ((i)&0xf)

const char *elf32_lookup_symbol (uint32_t addr, elf32_t *elf);
elf32_t arch_loader_parse_kernel_elf(struct multiboot *mb, elf32_t *elf);
int arch_loader_process_elf32_phdr(char *mem, int fp, addr_t *start, addr_t *end);

extern elf32_t kernel_elf;

#define MAX_SECTIONS 32

struct section_data {
	addr_t vbase[MAX_SECTIONS];
	int num;
};

#endif
