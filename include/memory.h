#ifndef MEMORY_H
#define MEMORY_H

#include <isr.h>
#include <multiboot.h>
#include <sys/stat.h>
#include <slab.h>
#include <config.h>
#include <sea/cpu/registers.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <memory-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <memory-x86_64.h>
#endif

typedef addr_t page_dir_t, page_table_t, pml4_t, pdpt_t;

struct pd_data {
	unsigned count;
	mutex_t lock;
};

extern struct pd_data *pd_cur_data;

extern volatile addr_t pm_location;
extern volatile addr_t pm_stack;
extern volatile addr_t pm_stack_max;
extern volatile unsigned long pm_num_pages, pm_used_pages;
extern volatile addr_t highest_page;
extern volatile addr_t lowest_page;
extern int memory_has_been_mapped;
extern volatile addr_t placement;
extern mutex_t pm_mutex;
extern volatile page_dir_t *kernel_dir, *current_dir;
extern int id_tables;

#define vm_unmap(x) vm_do_unmap(x, 0)
#define vm_unmap_only(x) vm_do_unmap_only(x, 0)
#define vm_getattrib(a, b) vm_do_getattrib(a, b, 0)
#define vm_getmap(a, b) vm_do_getmap(a, b, 0)
#define pm_alloc_page() __pm_alloc_page(__FILE__, __LINE__)

void free_thread_shared_directory();
void free_thread_specific_directory();
void page_fault(registers_t *r);
int vm_map_all(addr_t virt, addr_t phys, unsigned attr);
void vm_init(addr_t id_map_to);
void vm_switch(page_dir_t *n/*VIRTUAL ADDRESS*/);
int vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned);
int vm_do_unmap(addr_t virt, unsigned);
int vm_unmap_all(addr_t virt);
addr_t vm_do_getmap(addr_t v, addr_t *p, unsigned);
page_dir_t *vm_clone(page_dir_t *pd, char);
page_dir_t *vm_copy(page_dir_t *pd);
void process_memorymap(struct multiboot *mboot);
void pm_init(addr_t start, struct multiboot *);
addr_t __pm_alloc_page(char *, int);
void install_kmalloc(char *name, unsigned (*init)(addr_t, addr_t), 
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
addr_t do_kmalloc(size_t sz, char, char *, int);
void *__kmalloc(size_t s, char *, int);
void *__kmalloc_a(size_t s, char *, int);
void *__kmalloc_ap(size_t s, addr_t *, char *, int);
void *__kmalloc_p(size_t s, addr_t *, char *, int);
int vm_do_unmap_only(addr_t v, unsigned);
void pm_free_page(addr_t addr);
void vm_init_2();
extern addr_t i_stack;
void vm_init_tracking();
unsigned int vm_setattrib(addr_t v, short attr);
void setup_kernelstack();
extern void zero_page_physical(addr_t);
#define kmalloc(a) __kmalloc(a, __FILE__, __LINE__)
#define kmalloc_a(a) __kmalloc_a(a, __FILE__, __LINE__)
#define kmalloc_p(a,x) __kmalloc_p(a, x, __FILE__, __LINE__)
#define kmalloc_ap(a,x) __kmalloc_ap(a, x, __FILE__, __LINE__)

void __KT_swapper();
void copy_update_stack(addr_t old, addr_t new, unsigned length);
int __is_valid_user_ptr(int num, void *p, char flags);
static void map_if_not_mapped(addr_t loc)
{
	if(!vm_getmap(loc & PAGE_MASK, 0))
		vm_map(loc & PAGE_MASK, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
}

static void map_if_not_mapped_noclear(addr_t loc)
{
	if(!vm_getmap(loc & PAGE_MASK, 0))
		vm_map(loc & PAGE_MASK, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_NOCLEAR);
}

static void user_map_if_not_mapped(addr_t loc)
{
	if(!vm_getmap(loc & PAGE_MASK, 0))
		vm_map(loc & PAGE_MASK, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
	else
		vm_setattrib(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}

static void user_map_if_not_mapped_noclear(addr_t loc)
{
	if(!vm_getmap(loc & PAGE_MASK, 0))
		vm_map(loc & PAGE_MASK, __pm_alloc_page("map_if_not_mapped", 0), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT | MAP_NOCLEAR);
	else
		vm_setattrib(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}

#endif
