/* mm/clone.c: Copyright (c) 2010 Daniel Bittman
 * Handles cloning an address space */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <swap.h>
#include <cpu.h>

/* Accepts virtual, returns virtual */
int vm_do_copy_table(int i, page_dir_t *new, page_dir_t *from, char cow)
{

}

/* If is it normal task memory or the stack, we copy the tables. Otherwise we simply link them.
 */
int vm_copy_dir(page_dir_t *from, page_dir_t *new, char flags)
{

}
extern addr_t imps_lapic_addr;
/* Accepts virtual, returns virtual */
page_dir_t *vm_clone(page_dir_t *pd, char cow)
{

}

/* this sets up a new page directory in almost exactly the same way:
 * the directory is specific to the thread, but everything inside
 * the directory is just linked to the parent directory. The count
 * on the directory usage is increased, and the accounting page is 
 * linked so it can be accessed by both threads */
page_dir_t *vm_copy(page_dir_t *pd)
{

}
