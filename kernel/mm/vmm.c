#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <stdbool.h>
void arch_mm_context_clone(struct vmm_context *old, struct vmm_context *new);
void arch_mm_context_destroy(struct vmm_context *dir);
void arch_mm_free_userspace();

void arch_mm_flush_page_tables(void);

void mm_context_clone(struct vmm_context *old, struct vmm_context *new)
{
	return arch_mm_context_clone(old, new);
}

void mm_context_destroy(struct vmm_context *dir)
{
	arch_mm_context_destroy(dir);
}

void mm_free_userspace(void)
{
	arch_mm_free_userspace();
}

void mm_flush_page_tables(void)
{
	arch_mm_flush_page_tables();
}

bool arch_mm_context_virtual_map(struct vmm_context *ctx,
		addr_t virtual, addr_t physical, int flags, size_t length);

bool mm_context_virtual_map(struct vmm_context *ctx,
		addr_t virtual, addr_t physical, int flags, size_t length)
{
	return arch_mm_context_virtual_map(ctx, virtual, physical, flags, length);
}

bool arch_mm_context_write(struct vmm_context *ctx, addr_t address, void *src, size_t length);
bool mm_context_write(struct vmm_context *ctx, addr_t address, void *src, size_t length)
{
	return arch_mm_context_write(ctx, address, src, length);
}

addr_t arch_mm_context_virtual_unmap(struct vmm_context *ctx, addr_t address);
addr_t mm_context_virtual_unmap(struct vmm_context *ctx, addr_t address)
{
	return arch_mm_context_virtual_unmap(ctx, address);
}

bool arch_mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length);
bool mm_virtual_map(addr_t virtual, addr_t physical, int flags, size_t length)
{
	return arch_mm_virtual_map(virtual, physical, flags, length);
}

addr_t arch_mm_virtual_unmap(addr_t address);
addr_t mm_virtual_unmap(addr_t address)
{
	return arch_mm_virtual_unmap(address);
}

bool arch_mm_context_read(struct vmm_context *ctx, void *output,
		addr_t address, size_t length);
bool mm_context_read(struct vmm_context *ctx, void *output,
		addr_t address, size_t length)
{
	return arch_mm_context_read(ctx, output, address, length);
}


bool arch_mm_virtual_getmap(addr_t address, addr_t *phys, int *flags);
bool arch_mm_context_virtual_getmap(struct vmm_context *ctx, addr_t address, addr_t *phys, int *flags);

bool mm_virtual_getmap(addr_t address, addr_t *phys, int *flags)
{
	return arch_mm_virtual_getmap(address, phys, flags);
}

bool mm_context_virtual_getmap(struct vmm_context *ctx, addr_t address, addr_t *phys, int *flags)
{
	return arch_mm_context_virtual_getmap(ctx, address, phys, flags);
}


bool arch_mm_virtual_changeattr(addr_t virtual, int flags, size_t length);
bool arch_mm_context_virtual_changeattr(struct vmm_context *ctx, addr_t virtual, int flags, size_t length);

bool mm_virtual_changeattr(addr_t virtual, int flags, size_t length)
{
	return arch_mm_virtual_changeattr(virtual, flags, length);
}

bool mm_context_virtual_changeattr(struct vmm_context *ctx, addr_t virtual, int flags, size_t length)
{
	return arch_mm_context_virtual_changeattr(ctx, virtual, flags, length);
}

