#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <stdbool.h>
void arch_mm_vm_clone(struct vmm_context *old, struct vmm_context *new, struct thread *thr);
void arch_mm_destroy_directory(struct vmm_context *dir);
void arch_mm_free_self_directory(int);
void arch_mm_vm_init(addr_t id_map_to);
void arch_mm_vm_init_2();
void arch_mm_vm_switch_context(struct vmm_context *context);
addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
void arch_mm_vm_set_attrib(addr_t v, short attr);
unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked);
int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int arch_mm_vm_unmap_only(addr_t virt, unsigned locked);
int arch_mm_vm_unmap(addr_t virt, unsigned locked);
void arch_mm_flush_page_tables(void);

void mm_vm_clone(struct vmm_context *old, struct vmm_context *new, struct thread *thr)
{
	return arch_mm_vm_clone(old, new, thr);
}

void mm_destroy_directory(struct vmm_context *dir)
{
	arch_mm_destroy_directory(dir);
}

void mm_free_self_directory(int e)
{
	arch_mm_free_self_directory(e);
}

void mm_vm_init(addr_t id_map_to)
{
	arch_mm_vm_init(id_map_to);
}

addr_t mm_vm_get_map(addr_t v, addr_t *p, unsigned locked)
{
	return arch_mm_vm_get_map(v, p, locked);
}

void mm_vm_set_attrib(addr_t v, short attr)
{
	arch_mm_vm_set_attrib(v, attr);
}

unsigned int mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked)
{
	return arch_mm_vm_get_attrib(v, p, locked);
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

