#ifndef ELF_H
#define ELF_H
#include <mod.h>
#define d_memcmp memcmp

extern void * kernel_start;

struct __attribute__((packed)) ELF_section_header_s
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
} __attribute__((packed));

struct __attribute__((packed)) ELF_reloc_entry_s
{
	int   offset;
	uint32_t  info;
} __attribute__((packed));

typedef struct {
	uint32_t r_offset;
	uint32_t r_info;
} elf_rel_t;

struct __attribute__((packed)) ELF_symtab_entry_s
{
	uint32_t name;
	uint32_t address;
	uint32_t size;
	uint8_t  info;
	uint8_t  other;
	uint16_t shndx;
} __attribute__((packed));

typedef struct ELF_symtab_entry_s elf_syment_t;

typedef struct
{
	uint32_t p_type;
	uint32_t p_offset;
	uint32_t p_addr;
	uint32_t p_paddr;
	uint32_t p_filesz;
	uint32_t p_memsz;
	uint32_t p_flags;
	uint32_t p_align;
} elf_phdr_t;

#define PH_LOAD    1
#define PH_DYNAMIC 2
#define PH_INTERP  3


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

#define SHN_COMMON 0xFF02
#define SHN_ABS    0xFF01


#define ELF_R_NONE     0
#define ELF_R_32       1
#define ELF_R_PC32     2
#define ELF_R_GOT32    3
#define ELF_R_PLT32    4
#define ELF_R_COPY     5
#define ELF_R_GLOBDAT  6
#define ELF_R_JMPSLOT  7
#define ELF_R_RELATIVE 8
#define ELF_R_GOTOFF   9
#define ELF_R_GOTPC   10

#define ELF_R_SYM(i)  ((i)>>8)
#define ELF_R_TYPE(i) ((unsigned char)(i))

typedef struct {
	uint16_t d_tag;
	union {
		uint32_t d_val;
		uint32_t d_ptr;
	} d_un;
} elf_dyn_t;

typedef struct
{
  struct ELF_symtab_entry_s *symtab;
  uint32_t      symtabsz;
  const char   *strtab;
  uint32_t      strtabsz;
  unsigned lookable;
} elf_t;

#define GET_RELOC_SYM(i)  ((i)>>8)
#define GET_RELOC_TYPE(i) ((unsigned char)(i))

#define GET_SYMTAB_BIND(i)   ((i)>>4)
#define GET_SYMTAB_TYPE(i)   ((i)&0xf)

#define SHN_UNDEF   0
void _add_kernel_sym_user(const intptr_t func, const char * funcstr);
void _add_kernel_symbol(const intptr_t func, const char * funcstr);
intptr_t find_kernel_function_user(char * unres);
intptr_t find_kernel_function(char * unres);
void init_kernel_symbols(void);
int parse_elf_module(module_t *mod, uint8_t * buf, char *name);
const char *elf_lookup_symbol (uint32_t addr, elf_t *elf);
elf_t parse_kernel_elf(struct multiboot *mb, elf_t *);
extern elf_t kernel_elf;

#include <block.h>
unsigned long long get_epoch_time();
void remove_dfs_node(char *name);
int kernel_cache_sync_slow(int all);
int disconnect_block_cache(int dev);
int ttyx_ioctl(int min, int cmd, int arg);
void unregister_block_device(int n);
int write_block_cache(int dev, int blk);
long long block_read(int dev, unsigned long long posit, char *buf, unsigned int c);
long long block_write(int dev, unsigned long long posit, char *buf, unsigned int c);
struct inode *dfs_cn(char *name, int mode, int major, int minor);
int block_rw(int rw, int dev, int blk, char *buf, blockdevice_t *bd);
int sys_setsid();
int proc_set_callback(int major, int( *callback)(char rw, struct inode *inode, int m, char *buf));
int proc_get_major();
struct inode *pfs_cn_node(struct inode *to, char *name, int mode, int major, int minor);
struct inode *pfs_cn(char *name, int mode, int major, int minor);
int iremove(struct inode *i);
void delay_sleep(int t);
int block_ioctl(int dev, int cmd, int arg);
#endif
