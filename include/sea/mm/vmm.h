#ifndef __SEA_MM_VMM_H
#define __SEA_MM_VMM_H

#include <sea/mm/pmm.h>
#include <sea/arch-include/mm-memory.h>
#include <sea/mm/valloc.h>
#include <stdbool.h>

struct thread;
struct pd_data {
	unsigned count;
	mutex_t lock;
};

struct vmm_context {
	uint32_t magic;
	addr_t root_physical;
	addr_t root_virtual;
	mutex_t lock;
};

#define MAP_ZERO    0x100000
#define __ALL_ATTRS MAP_ZERO
_Static_assert((__ALL_ATTRS & ATTRIB_MASK) == 0,
		"tried to redefine paging attribute");

#define CONTEXT_MAGIC 0xC047387F

extern struct vmm_context kernel_context;

#define pd_cur_data (current_process ? &current_process->vmm_context : 0)

extern int id_tables;
extern addr_t initial_boot_stack; /* TODO: don't we have another one of these? */
void mm_vm_clone(struct vmm_context *, struct vmm_context *, struct thread *);
void mm_vm_switch_context(struct vmm_context *);

void mm_vm_init(addr_t id_map_to);
void mm_vm_init_2();
addr_t mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
unsigned int mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked);
int mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int mm_is_valid_user_pointer(int num, void *p, char flags);
void mm_page_fault_handler(registers_t *, addr_t, int);
void mm_flush_page_tables();
void mm_destroy_directory(struct vmm_context *dir);
void mm_free_self_directory(int);

bool mm_context_virtual_map(struct vmm_context *ctx,
		addr_t virtual, addr_t physical, int flags, size_t length);
bool mm_context_write(struct vmm_context *ctx, addr_t address, void *src, size_t length);
bool mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length);
addr_t mm_virtual_unmap(addr_t address);
bool mm_context_read(struct vmm_context *ctx, void *output,
		addr_t address, size_t length);
bool mm_virtual_getmap(addr_t address, addr_t *phys, int *flags);
bool mm_context_virtual_getmap(struct vmm_context *ctx, addr_t address, addr_t *phys, int *flags);
bool mm_context_virtual_changeattr(struct vmm_context *ctx, addr_t virtual, int flags, size_t length);
bool mm_virtual_changeattr(addr_t virtual, int flags, size_t length);

addr_t mm_context_virtual_unmap(struct vmm_context *ctx, addr_t address);
static inline void map_if_not_mapped(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0)) {
		addr_t phys = mm_physical_allocate(0x1000, true);
		if(!mm_virtual_map(loc & PAGE_MASK, phys, PAGE_PRESENT | PAGE_WRITE, 0x1000)) {
			mm_physical_deallocate(phys);
		}
	}
}

static inline void map_if_not_mapped_noclear(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0)) {
		addr_t phys = mm_physical_allocate(0x1000, false);
		if(!mm_virtual_map(loc & PAGE_MASK, phys, PAGE_PRESENT | PAGE_WRITE, 0x1000))
			mm_physical_deallocate(phys);
	}
}

static inline void user_map_if_not_mapped(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0)) {
		addr_t phys = mm_physical_allocate(0x1000, true);
		if(!mm_virtual_map(loc & PAGE_MASK, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000)) {
			mm_physical_deallocate(phys);
			mm_virtual_changeattr(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000);
		}
	} else
		mm_virtual_changeattr(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000);
}

static inline void user_map_if_not_mapped_noclear(addr_t loc)
{
	if(!mm_vm_get_map(loc & PAGE_MASK, 0, 0)) {
		addr_t phys = mm_physical_allocate(0x1000, false);
		if(!mm_virtual_map(loc & PAGE_MASK, phys, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000)) {
			mm_physical_deallocate(phys);
			mm_virtual_changeattr(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000);
		}
	} else
		mm_virtual_changeattr(loc & PAGE_MASK, PAGE_PRESENT | PAGE_WRITE | PAGE_USER, 0x1000);
}

#define PF_CAUSE_NONPRESENT  1
#define PF_CAUSE_READ         2
#define PF_CAUSE_WRITE        4
#define PF_CAUSE_IFETCH       8
#define PF_CAUSE_RSVD      0x10
#define PF_CAUSE_USER      0x20
#define PF_CAUSE_SUPER     0x40

#endif
