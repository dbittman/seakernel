#ifndef __SEA_MM_VMM_H
#define __SEA_MM_VMM_H

#include <sea/mm/pmm.h>
#include <sea/arch-include/mm-memory.h>
#include <sea/mm/valloc.h>
#include <stdbool.h>
#include <sea/spinlock.h>

struct vmm_context {
	addr_t root_physical;
	addr_t root_virtual;
	struct spinlock lock;
	uint32_t magic;
};

#define MAP_ZERO    0x100000
#define __ALL_ATTRS MAP_ZERO
_Static_assert((__ALL_ATTRS & ATTRIB_MASK) == 0,
		"tried to redefine paging attribute");

#define CONTEXT_MAGIC 0xC047387F

extern struct vmm_context kernel_context;

extern addr_t initial_boot_stack; /* TODO: don't we have another one of these? */

int mm_is_valid_user_pointer(int num, void *p, char flags);

void mm_page_fault_handler(struct registers *, addr_t, int);
void mm_page_fault_init(void);
void mm_flush_page_tables();

void mm_context_clone(struct vmm_context *, struct vmm_context *);
void mm_context_destroy(struct vmm_context *dir);
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
bool mm_context_virtual_trymap(struct vmm_context *ctx, addr_t virtual, int flags, size_t length);
bool mm_virtual_trymap(addr_t virtual, int flags, size_t length);


#define PF_CAUSE_NONPRESENT   1
#define PF_CAUSE_READ         2
#define PF_CAUSE_WRITE        4
#define PF_CAUSE_IFETCH       8
#define PF_CAUSE_RSVD      0x10
#define PF_CAUSE_USER      0x20
#define PF_CAUSE_SUPER     0x40

#endif
