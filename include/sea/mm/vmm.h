#ifndef __SEA_MM_VMM_H
#define __SEA_MM_VMM_H

#include <sea/mm/pmm.h>
#include <sea/arch-include/mm-memory.h>
struct thread;
struct pd_data {
	unsigned count;
	mutex_t lock;
};

struct vmm_context {
	addr_t root_physical;
	addr_t root_virtual;
	mutex_t lock;
};

extern struct vmm_context kernel_context;

#define pd_cur_data (current_process ? &current_process->vmm_context : 0)

extern int id_tables;
extern addr_t initial_boot_stack; /* TODO: don't we have another one of these? */
void mm_vm_clone(struct vmm_context *, struct vmm_context *);
void mm_vm_switch_context(struct vmm_context *);

void mm_vm_init(addr_t id_map_to);
void mm_vm_init_2();
addr_t mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
void mm_vm_set_attrib(addr_t v, short attr);
unsigned int mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked);
int mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int mm_vm_unmap_only(addr_t virt, unsigned locked);
int mm_vm_unmap(addr_t virt, unsigned locked);
int mm_is_valid_user_pointer(int num, void *p, char flags);
void mm_page_fault_handler(registers_t *, addr_t, int);
void mm_flush_page_tables();

static void map_if_not_mapped(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0))
		mm_vm_map(loc & PAGE_MASK, mm_alloc_physical_page(), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT);
}

static void map_if_not_mapped_noclear(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0))
		mm_vm_map(loc & PAGE_MASK, mm_alloc_physical_page(), 
		       PAGE_PRESENT | PAGE_WRITE, MAP_CRIT | MAP_NOCLEAR);
}

static void user_map_if_not_mapped(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0))
		mm_vm_map(loc & PAGE_MASK, mm_alloc_physical_page(), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT);
	else
		mm_vm_set_attrib(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}

static void user_map_if_not_mapped_noclear(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0))
		mm_vm_map(loc & PAGE_MASK, mm_alloc_physical_page(), 
		       PAGE_PRESENT | PAGE_WRITE | PAGE_USER, MAP_CRIT | MAP_NOCLEAR);
	else
		mm_vm_set_attrib(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
}

#define PF_CAUSE_NONPRESENT  1
#define PF_CAUSE_READ         2
#define PF_CAUSE_WRITE        4
#define PF_CAUSE_IFETCH       8
#define PF_CAUSE_RSVD      0x10
#define PF_CAUSE_USER      0x20
#define PF_CAUSE_SUPER     0x40

#endif
