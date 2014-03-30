#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/mm/swap.h>
#include <sea/dm/block.h>
int get_empty_slot(swapdev_t *s)
{
	unsigned i;
	for(i=1;i<s->nslots;i++)
	{
		if(!s->block_index[i]) {
			s->block_index[i]=1;
			return i;
		}
	}
	return 0;
}

/* This does not unmap!! */
int swap_page_to_device(task_t *t, swapdev_t *s, unsigned addr /* includes attributes */)
{
	mutex_on(s->lock);
	page_index_t *pi = s->page_index;
	unsigned i;
	for(i=0;i<s->nslots;i++)
	{
		if(pi->page == 0 || pi->page == 0xFFFFFFFF)
			break;
		pi++;
	}
	unsigned slot;
	if(i == s->nslots || ((slot = get_empty_slot(s)) == 0)) {
		mutex_off(s->lock);
		return -1;
	}
	if((pi->page == 0xFFFFFFFF) && ((i+1) < s->nslots))
	{
		page_index_t *n = pi+1;
		assert(!n->page);
		n->page = 0xFFFFFFFF;
	}
	pi->page = addr;
	pi->pid = t->pid;
	pi->slot = slot;
	/* Ok, now write out the data */
	unsigned ss = 0x1000 / s->blocksize;
	for(i=0;i<ss;i++)
		dm_do_block_rw(WRITE, s->dev, (unsigned)(pi->slot * ss) + i, 
			(char *)((addr & PAGE_MASK) + i*s->blocksize), 0);
	s->bytes_used += 0x1000;
	s->uslots += 1;
	mutex_off(s->lock);
	return 0;
}

int swap_page_out(task_t *t, unsigned addr)
{
	if(!valid_swappable(addr))
		panic(PANIC_MEM | PANIC_NOSYNC, "Tried to swap invalid page (%x)!", addr);
	t->flags |= TF_SWAP;
	unsigned attr;
	if(!vm_getattrib(addr, &attr)) {
		t->flags &= ~TF_SWAP;
		return -1;
	}
#ifdef SWAP_DEBUG
	printk(0, "Attempting to swap page %d:%x\n", t->pid, addr);
#endif
	addr |= attr;
	swapdev_t *s = swaplist;
	while(s)
	{
		if(s->uslots < s->nslots)
		{
			if(!s->lock->count)
			{
				mutex_on(s->lock);
				if(swap_page_to_device(t, s, addr) == -1)
				{
					mutex_off(s->lock);
					goto nex;
				}
				/* NOTE: We do NOT call this. Let the manager do the unmapping */
				//vm_unmap(addr);
				mutex_off(s->lock);
				t->flags &= ~TF_SWAP;
				t->num_swapped++;
#ifdef SWAP_DEBUG
				printk(0, "Success\n");
#endif
				return 0;
			}
		}
		nex:
		s=s->next;
	}
	t->flags &= ~TF_SWAP;
#ifdef SWAP_DEBUG
	printk(0, "failed\n");
#endif
	return -1;
}
