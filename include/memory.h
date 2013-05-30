#ifndef MEMORY_H
#define MEMORY_H

#include <kernel.h>
#include <multiboot.h>
#include <isr.h>
#include <sys/stat.h>
#include <slab.h>

#define PAGE_MASK      0xFFFFF000
#define ATTRIB_MASK    0x00000FFF
#define PAGE_PRESENT   0x1
#define PAGE_WRITE     0x2
#define PAGE_USER      0x4
#define PAGE_WRITECACHE 0x8
#define PAGE_NOCACHE   0x10
#define PAGE_COW       512
#define PAGE_SIZE 	   0x1000

#define MAP_NOIPI     0x8
#define MAP_PDLOCKED  0x4
#define MAP_NOCLEAR   0x2
#define MAP_CRIT      0x1
#define MAP_NORM      0x0

#define PAGE_DIR_IDX(x) ((uint32_t)x/1024)
#define PAGE_TABLE_IDX(x) ((uint32_t)x%1024)
#define PAGE_DIR_PHYS(x) (x[1023]&PAGE_MASK)

#define page_directory ((unsigned *)DIR_PHYS)
#define page_tables ((unsigned *)TBL_PHYS)
typedef unsigned int page_dir_t;

struct pd_data {
	unsigned count;
	mutex_t lock;
};

extern struct pd_data *pd_cur_data;

extern volatile addr_t pm_location;
extern volatile addr_t pm_stack;
extern volatile addr_t pm_stack_max;
extern volatile unsigned pm_num_pages, pm_used_pages;
extern volatile addr_t highest_page;
extern volatile addr_t lowest_page;
extern int memory_has_been_mapped;
extern volatile addr_t placement;
extern mutex_t pm_mutex;
extern volatile page_dir_t *kernel_dir, *current_dir;
extern int id_tables;

#define disable_paging() \
	__asm__ volatile ("mov %%cr0, %0" : "=r" (cr0temp)); \
	cr0temp &= ~0x80000000; \
	__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0temp));

#define enable_paging() \
	__asm__ volatile ("mov %%cr0, %0" : "=r" (cr0temp)); \
	cr0temp |= 0x80000000; \
	__asm__ volatile ("mov %0, %%cr0" : : "r" (cr0temp));

#define GET_PDIR_INFO(x) (page_dir_info *)(t_page + x*sizeof(page_dir_info))

#define flush_pd() \
 __asm__ __volatile__("movl %%cr3,%%eax\n\tmovl %%eax,%%cr3": : :"ax", "eax")

#define vm_unmap(x) vm_do_unmap(x, 0)
#define vm_unmap_only(x) vm_do_unmap_only(x, 0)

#define vm_getattrib(a, b) vm_do_getattrib(a, b, 0)
#define vm_getmap(a, b) vm_do_getmap(a, b, 0)

void page_fault(registers_t *r);
int vm_map_all(addr_t virt, addr_t phys, unsigned attr);
void vm_init(unsigned id_map_to);
void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/);
int vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned);
int vm_do_unmap(addr_t virt, unsigned);
int vm_unmap_all(addr_t virt);
unsigned int vm_do_getmap(addr_t v, addr_t *p, unsigned);
page_dir_t *vm_clone(page_dir_t *pd, char);
page_dir_t *vm_copy(page_dir_t *pd);
void process_memorymap(struct multiboot *mboot);
void pm_init(int start, struct multiboot *);
int free_stack();
addr_t __pm_alloc_page(char *, int);
#define pm_alloc_page() __pm_alloc_page(__FILE__, __LINE__)
void install_kmalloc(char *name, unsigned (*init)(unsigned, unsigned), 
	addr_t (*alloc)(size_t, char), void (*free)(void *));
addr_t do_kmalloc_slab(size_t sz, char align);
void do_kfree_slab(void *ptr);
unsigned slab_init(addr_t start, addr_t end);
void pm_free_page(addr_t addr);
unsigned int vm_do_getattrib(addr_t v, unsigned *p, unsigned);
extern void copy_page_physical(addr_t, addr_t);
extern void copy_page_physical_half(addr_t, addr_t);
int allocate_dma_buffer(size_t length, addr_t *virtual, addr_t *physical);
int vm_cleanup(page_dir_t *, unsigned *);
int pm_stat_mem(struct mem_stat *s);
void vm_free_page_table(int tbl, addr_t *t/* VIRTUAL ADDRESS */, addr_t *);
void kfree(void * pt);
addr_t do_kmalloc(size_t sz, char);
addr_t __kmalloc(size_t s, char *, int);
addr_t kmalloc_a(size_t s);
addr_t kmalloc_ap(size_t s, unsigned *);
addr_t kmalloc_p(size_t s, unsigned *);
int self_free(int);
int vm_do_unmap_only(addr_t v, unsigned);
int self_free_table(int t);
void pm_free_page(addr_t addr);
void vm_init_2();
void vm_init_tracking();
unsigned int vm_setattrib(unsigned v, short attr);
void setup_kernelstack(int);
extern void zero_page_physical(addr_t);
#define kmalloc(a) __kmalloc(a, __FILE__, __LINE__)
void __KT_swapper();
extern void copy_update_stack(unsigned old, unsigned new, unsigned length);
static inline void map_if_not_mapped(addr_t loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
}

static inline void map_if_not_mapped_noclear(addr_t loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_NOCLEAR);
}

static inline void user_map_if_not_mapped(addr_t loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
}

static inline void user_map_if_not_mapped_noclear(addr_t loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT | MAP_NOCLEAR);
}

#endif
