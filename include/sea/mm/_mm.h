#ifndef _SEA_MM__MM_H
#define _SEA_MM__MM_H

#include <sea/types.h>
#include <sea/tm/process.h>
#include <sea/mm/context.h>

unsigned __mm_slab_init(addr_t start, addr_t end);
void __mm_do_kfree_slab(void *ptr);
addr_t __mm_do_kmalloc_slab(size_t sz, char align);


vmm_context_t *arch_mm_vm_clone(vmm_context_t *pd, char cow);
vmm_context_t *arch_mm_vm_copy(vmm_context_t *pd);
void arch_mm_free_thread_shared_directory();
void arch_mm_destroy_task_page_directory(task_t *p);
void arch_mm_free_thread_specific_directory();

void arch_mm_copy_page_physical(addr_t a, addr_t b);
void arch_mm_zero_page_physical(addr_t a);

void arch_mm_free_physical_page(addr_t addr);
addr_t arch_mm_alloc_physical_page();
void arch_mm_pm_init();

void arch_mm_vm_init(addr_t id_map_to);
void arch_mm_vm_init_2();
void arch_mm_vm_switch_context(vmm_context_t *n/*VIRTUAL ADDRESS*/);
addr_t arch_mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
void arch_mm_vm_set_attrib(addr_t v, short attr);
unsigned int arch_mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked);
int arch_mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int arch_mm_vm_unmap(addr_t virt, unsigned locked);
int arch_mm_vm_unmap_only(addr_t virt, unsigned locked);

#endif
