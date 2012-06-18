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
#define PAGE_COW       512
#define PAGE_SIZE 0x1000

#define MAP_NOCLEAR   0x2
#define MAP_CRIT      0x1
#define MAP_NORM      0x0

#define PAGE_DIR_IDX(x) ((uint32_t)x/1024)
#define PAGE_TABLE_IDX(x) ((uint32_t)x%1024)
#define PAGE_DIR_PHYS(x) (x[1023]&PAGE_MASK)
typedef unsigned int page_dir_t;

extern volatile unsigned int pm_location;
extern volatile unsigned int pm_stack;
extern volatile unsigned int pm_stack_max;
extern volatile unsigned pm_num_pages, pm_used_pages;
extern volatile unsigned highest_page;
extern volatile unsigned lowest_page;
extern int memory_has_been_mapped;
extern volatile int placement;
extern mutex_t pm_mutex;
extern volatile int mmu_ready;
extern char paging_enabled;
extern volatile page_dir_t *kernel_dir, *current_dir;
extern unsigned *page_directory, *page_tables;
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

void page_fault(registers_t r);
int vm_map_all(unsigned virt, unsigned phys, unsigned attr);
void vm_init(unsigned id_map_to);
void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/);
int vm_map(unsigned virt, unsigned phys, unsigned attr, unsigned);
int vm_map_all(unsigned virt, unsigned phys, unsigned attr);
int vm_unmap(unsigned virt);
int vm_unmap_all(unsigned virt);
unsigned int vm_getmap(unsigned v, unsigned *p);
page_dir_t *vm_clone(page_dir_t *pd, char);
void process_memorymap(struct multiboot *mboot);
void pm_init(int start, struct multiboot *);
int free_stack();
int __pm_alloc_page(char *, int);
#define pm_alloc_page() __pm_alloc_page(__FILE__, __LINE__)
unsigned int do_kmalloc_heap(unsigned sz, char align);
void do_kfree_heap(void *pt);
unsigned do_kmalloc_wave(unsigned size, char align);
unsigned wave_init(unsigned start, unsigned end);
void install_kmalloc(char *name, unsigned (*init)(unsigned, unsigned), 
	unsigned (*alloc)(unsigned, char), void (*free)(void *));
void do_kfree_wave(void *ptr);
unsigned do_kmalloc_slab(unsigned sz, char align);
void do_kfree_slab(void *ptr);
unsigned slab_init(unsigned start, unsigned end);
void pm_free_page(unsigned int addr);
unsigned int vm_getattrib(unsigned v, unsigned *p);
extern void copy_page_physical(unsigned int, unsigned int);
extern void copy_page_physical_half(unsigned int, unsigned int);
int vm_cleanup(page_dir_t *, unsigned *);
int pm_stat_mem(struct mem_stat *s);
void vm_free_page_table(int tbl, unsigned int *t/* VIRTUAL ADDRESS */, unsigned int*);
void alloc_mas_heap(unsigned start, int len);
void kfree(void * pt);
unsigned int do_kmalloc(unsigned sz, char);
unsigned __kmalloc(unsigned s, char *, int);
unsigned kmalloc_a(unsigned s);
unsigned kmalloc_ap(unsigned s, unsigned *);
unsigned kmalloc_p(unsigned s, unsigned *);
int self_free(int);
int vm_unmap_only(unsigned v);
int self_free_table(int t);
void pm_free_page(unsigned int addr);
void vm_init_2();
void vm_init_tracking();
void setup_kernelstack(int);
extern void zero_page_physical(unsigned);
#define kmalloc(a) __kmalloc(a, __FILE__, __LINE__)

static inline void map_if_not_mapped(unsigned loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
}

static inline void map_if_not_mapped_noclear(unsigned loc)
{
	if(!vm_getmap(loc & 0xFFFFF000, 0))
		vm_map(loc & 0xFFFFF000, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT | MAP_NOCLEAR);
}

#endif
