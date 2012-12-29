/* Forking of processes. */
#include <kernel.h>
#include <memory.h>
#include <task.h>
extern void copy_update_stack(unsigned old, unsigned new, unsigned length);
