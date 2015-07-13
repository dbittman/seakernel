#include <sea/types.h>
#include <sea/mm/vmm.h>

void mm_vm_clone(struct vmm_context *old, struct vmm_context *new)
{
	return arch_mm_vm_clone(old, new);
}

void mm_destroy_directory(struct vmm_context *dir)
{

}

void mm_free_self_directory(void)
{

}

void mm_vm_init(addr_t id_map_to)
{
	arch_mm_vm_init(id_map_to);
}

void mm_vm_init_2(void)
{
	arch_mm_vm_init_2();
}

void mm_vm_switch_context(struct vmm_context *c)
{
	arch_mm_vm_switch_context(c);
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

int mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt)
{
	return arch_mm_vm_map(virt, phys, attr, opt);
}

int mm_vm_unmap_only(addr_t virt, unsigned locked)
{
	return arch_mm_vm_unmap_only(virt, locked);
}

int mm_vm_unmap(addr_t virt, unsigned locked)
{
	return arch_mm_vm_unmap(virt, locked);
}

void mm_flush_page_tables(void)
{
	arch_mm_flush_page_tables();
}

