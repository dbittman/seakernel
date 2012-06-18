#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <swap.h>
#include <block.h>

swapdev_t *swap_find_page(unsigned addr, unsigned pid, page_index_t **ret)
{
	if(!addr || !pid)
		panic(PANIC_MEM | PANIC_NOSYNC, "Invalid call to swap_find_page");
	swapdev_t *s = swaplist;
	while(s)
	{
		/* Scan this device for the page */
		mutex_on(s->lock);
		page_index_t *pi = s->page_index;
		unsigned int i = 0;
		for(i=0;i<s->nslots;i++)
		{
			if(pi->page == 0xFFFFFFFF)
				goto next; /* Last page */
			if(((pi->page & PAGE_MASK) == (addr & PAGE_MASK)) && pi->pid == pid)
			{
				*ret = pi;
				mutex_off(s->lock);
				return s;
			}
			pi++;
		}
		next:
		mutex_off(s->lock);
		s=s->next;
	}
	return 0;
}

int do_swap_in_page(task_t *t, unsigned addr, page_index_t *pi, swapdev_t *s)
{
	if(!s || !pi)
	{
		t->flags &= ~TF_SWAP;
#ifdef SWAP_DEBUG
		printk(0, "Failed\n");
#endif
		return -1;
	}
	mutex_on(s->lock);
	unsigned page = pm_alloc_page();
	vm_map(addr & PAGE_MASK, page, pi->page & ATTRIB_MASK, 0);
	int ss = 0x1000 / s->blocksize;
	int i;
	for(i=0;i<ss;i++)
		do_block_rw(READ, s->dev, pi->slot * ss + i, 
			(char *)((addr & PAGE_MASK) + i*s->blocksize), 0);
	pi->page = pi->pid = 0;
	s->block_index[pi->slot]=0;
	s->bytes_used -= 0x1000;
	s->uslots -= 1;
	mutex_off(s->lock);
	t->flags &= ~TF_SWAP;
	t->num_swapped--;
#ifdef SWAP_DEBUG
	printk(0, "Success!\n");
#endif
	return 0;
}

int swap_in_page(task_t *t, unsigned addr)
{
	page_index_t *pi=0;
	t->flags |= TF_SWAP;
#ifdef SWAP_DEBUG
	printk(0, "Attempting to read back page %d:%x\n", t->pid, addr);
#endif
	swapdev_t *s = swap_find_page(addr, t->pid, &pi);
	return do_swap_in_page(t, addr, pi, s);
}

int swap_in_all_the_pages(task_t *t)
{
	if(!t)
		panic(PANIC_MEM | PANIC_NOSYNC, "Invalid call to swap_in_all_the_pages");
	if(!t->num_swapped)
		return 0;
	swapdev_t *s = swaplist;
	while(s)
	{
		/* Scan this device for the page */
		mutex_on(s->lock);
		page_index_t *pi = s->page_index;
		unsigned int i = 0;
		for(i=0;i<s->nslots;i++)
		{
			if(pi->page == 0xFFFFFFFF)
				goto next; /* Last page */
			if(pi->pid == t->pid) {
				unsigned addr = pi->page & PAGE_MASK;
				do_swap_in_page(t, addr, pi, s);
				if(!t->num_swapped) {
					mutex_off(s->lock);
					return 0;
				}
			}
			pi++;
		}
		next:
		mutex_off(s->lock);
		s=s->next;
	}
	return 0;
}
