#ifndef __SEA_MM_VMM_H
#define __SEA_MM_VMM_H

#include <sea/mm/context.h>
#include <sea/tm/process.h>

typedef addr_t page_dir_t, page_table_t, pml4_t, pdpt_t;

page_dir_t *mm_vm_clone(page_dir_t *pd, char cow);
page_dir_t *mm_vm_copy(page_dir_t *pd);
void mm_free_thread_shared_directory();
void mm_destroy_task_page_directory(task_t *p);
void mm_free_thread_specific_directory();
void mm_vm_init(addr_t id_map_to);
void mm_vm_init_2();
void mm_vm_switch_context(page_dir_t *n/*VIRTUAL ADDRESS*/);
addr_t mm_vm_get_map(addr_t v, addr_t *p, unsigned locked);
void mm_vm_set_attrib(addr_t v, short attr);
unsigned int mm_vm_get_attrib(addr_t v, unsigned *p, unsigned locked);
int mm_vm_map(addr_t virt, addr_t phys, unsigned attr, unsigned opt);
int mm_vm_unmap_only(addr_t virt, unsigned locked);
int mm_vm_unmap(addr_t virt, unsigned locked);

#endif
