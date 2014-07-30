#include <sea/mm/_mm.h>
#include <sea/mm/context.h>

vmm_context_t *mm_vm_clone(vmm_context_t *pd, char cow)
{
	return arch_mm_vm_clone(pd, cow);
}

vmm_context_t *mm_vm_copy(vmm_context_t *pd)
{
	return arch_mm_vm_copy(pd);
}

void mm_free_thread_shared_directory()
{
	arch_mm_free_thread_shared_directory();
}

void mm_destroy_task_page_directory(task_t *p)
{
	arch_mm_destroy_task_page_directory(p);
}

void mm_free_thread_specific_directory()
{
	arch_mm_free_thread_specific_directory();
}

void mm_vm_init(addr_t id_map_to)
{
	return arch_mm_vm_init(id_map_to);
}

void mm_vm_init_2()
{
	return arch_mm_vm_init_2();
}

void mm_vm_switch_context(vmm_context_t *n/*VIRTUAL ADDRESS*/)
{
	arch_mm_vm_switch_context(n);
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

void mm_flush_page_tables()
{
	arch_mm_flush_page_tables();
}

