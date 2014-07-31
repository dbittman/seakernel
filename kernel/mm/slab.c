/* slab.c (kernel/mm): Implementation on a SLAB allocation system.
 * Implementation is Copyright (c) 2010 Daniel Bittman. 
 * 
 * Note: This is not a pure SLAB allocator! It is simply based off the SLAB design 
 * heavily, with only minor modifications to allow easier implementation.
 * First implementation completed October 13th, 2010
 * 
 * Diagram of memory layout:
 * | = page bound
 * start                      --Slabs--->                      end
 * {----|----|----|----|----|----|----|----|----|----|----|----}
 *   ^--slab cache list
 *         ^---^----^----^----Area allocation index
 * 
 */ 
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/slab.h>
#include <sea/mm/vmem.h>

#define SLAB_NUM_INDEX 120

static addr_t slab_start=0, slab_end=0;
static vma_t slab_area_alloc;

static slab_cache_t *scache_list[NUM_SCACHES];
static unsigned pages_used=0;

static unsigned num_slab=0, num_scache=0;
static void release_slab(slab_t *slab);
static mutex_t scache_lock;
//#define SLAB_DEBUG 1
#ifdef SLAB_DEBUG
static unsigned total=0;
#endif

static vnode_t *alloc_slab(unsigned np)
{
	assert(np);
	vnode_t *n = vmem_insert_node(&slab_area_alloc, np);
	if(!n)
		return 0;
	assert(n->num_pages >= np);
	pages_used += np;
	add_atomic(&num_slab, 1);
	return n;
}

static void free_slab(slab_t *slab)
{
	unsigned num_pages = slab->num_pages;
#ifdef SLAB_DEBUG
	printk(0, "[slab]: free slab %x - %x\n", (addr_t)slab, (addr_t)slab + num_pages * 0x1000);
#endif
	assert(slab);
	pages_used -= slab->num_pages;
	vnode_t *t = slab->vnode;
	slab->magic = 0;
	addr_t j;
	addr_t addr = (addr_t)slab;
	for(j=addr;j<(addr + num_pages*PAGE_SIZE);j+=PAGE_SIZE) {
		if(mm_vm_get_map(j, 0, 0))
			mm_vm_unmap(j, 0);
	}
	vmem_remove_node(&slab_area_alloc, t);
	sub_atomic(&num_slab, 1);
}

static slab_cache_t *get_empty_scache(int size, unsigned short flags)
{
	assert(size);
	unsigned i=0;
	mutex_acquire(&scache_lock);
	while(i < NUM_SCACHES)
	{
		if(((slab_cache_t *)(scache_list[i]))->id == -1)
			break;
		i++;
	}
	/* Normally, we would panic. However, the allocator has a 
	 * condition which might allow us to get around this. */
	if(i == NUM_SCACHES) {
		mutex_release(&scache_lock);
		return 0;
	}
	memset(((slab_cache_t *)(scache_list[i])), 0, sizeof(slab_cache_t));
	((slab_cache_t *)(scache_list[i]))->id = i;
	((slab_cache_t *)(scache_list[i]))->flags = flags;
	((slab_cache_t *)(scache_list[i]))->obj_size = size;
	add_atomic(&num_scache, 1);
	mutex_release(&scache_lock);
	return scache_list[i];
}

static void release_scache(slab_cache_t *sc)
{
	assert(sc);
	assert(!sc->full && !sc->partial && !sc->empty);
	mutex_acquire(&scache_lock);
	sc->id=-1;
	sub_atomic(&num_scache, 1);
	mutex_release(&scache_lock);
}

/* Adds a slab to a specified list in the scache. Doesn't remove the slab 
 * from it's old list though. WARNING: Before calling this, remove the 
 * slab from the current list first, Otherwise, we might corrupt the 
 * current list */
static int add_slab_to_list(slab_t *slab, char to)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	assert(slab->next==0 && slab->prev==0);
	slab_t **list=0;
	slab_cache_t *sc = (slab_cache_t *)(slab->parent);
	assert(sc);
	switch(to)
	{
		case TO_EMPTY:
			list=&sc->empty; 
			break;
		case TO_PARTIAL:
			list=&sc->partial; 
			break;
		case TO_FULL:
			list=&sc->full; 
			break;
		default:
			panic(PANIC_MEM | PANIC_NOSYNC, 
					"Invalid tranfer code sent to add_slab_to_list!");
			break;
	}
	assert(list);
	/* Woah, double pointers! While confusing, they make 
	 * things _much_ easier to handle. */
	slab_t *old = *list;
	if(old) assert(old->magic == SLAB_MAGIC);
	*list = slab;
	if(old) old->prev = slab;
	slab->next = old;
	slab->prev=0;
	return 0;
}

static void remove_slab_list(slab_t *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	slab_cache_t *sc = (slab_cache_t *)(slab->parent);
	assert(sc);
	if(slab->prev)
		slab->prev->next=slab->next;
	if(slab->next)
		slab->next->prev = slab->prev;
	/* If the lists of the cache point to this slab, we need to clear that... */
	slab_t **list=0;
	if(sc->empty == slab)
		list = &sc->empty;
	else if(sc->partial == slab)
		list = &sc->partial;
	else if(sc->full == slab)
		list = &sc->full;
	if(list) {
		*list = (slab_t *)slab->next;
		if(!*list) assert(!slab->prev);
	}
	slab->prev = slab->next=0;
}

static slab_t *create_slab(slab_cache_t *sc, int num_pages, unsigned short flags)
{
	assert(sc && num_pages);
	vnode_t *vnode=0;
	addr_t addr=0;
	vnode = alloc_slab(num_pages);
	if(!vnode)
		panic(PANIC_MEM | PANIC_NOSYNC, "alloc_slab: Unable to allocate slab");
	if(vnode)
		addr = vnode->addr;
	if(!addr)
		panic(PANIC_MEM | PANIC_NOSYNC, "create_slab: Unable to allocate slab");
	sc->slab_count++;
	unsigned i;
	addr_t j;
	for(j=addr;j<(addr + num_pages*PAGE_SIZE);j+=PAGE_SIZE) {
		if(!mm_vm_get_map(j, 0, 0))
			mm_vm_map(j, mm_alloc_physical_page(), PAGE_PRESENT, MAP_CRIT);
	}
	slab_t *slab = (slab_t *)addr;
	assert(slab->magic != SLAB_MAGIC);
	memset(slab, 0, sizeof(slab_t));
	slab->magic = SLAB_MAGIC;
	slab->flags = flags;
	slab->parent = (addr_t)sc;
	slab->num_pages = num_pages;
	slab->vnode = vnode;
	/* If we are aligning, we need to make number of objects 
	 * include the page aligned info page */
	if(flags & S_ALIGN)
		slab->obj_num = (((num_pages-1)*PAGE_SIZE)) / sc->obj_size;
	else
		slab->obj_num = ((num_pages*PAGE_SIZE)-sizeof(slab_t)) / sc->obj_size;
	if(slab->obj_num > MAX_OBJ_ID)
		slab->obj_num = MAX_OBJ_ID;
	slab->stack = (unsigned short *)slab->stack_arr;
	/* Setup the stack of objects */
	i=0;
	while(i < slab->obj_num && i<0xFFFF)
	{
		(*slab->stack) = i++;
		slab->stack++;
	}
	unsigned short *tmp = (unsigned short *)slab->stack;
	while(i < MAX_OBJ_ID)
	{
		(*tmp) = 0xFFFF;
		i++;
		tmp++;
	}
#ifdef SLAB_DEBUG
	printk(0, "[slab]: created new slab: %x - %x: %x %x %x %x\n", addr, addr + num_pages * 0x1000, flags, sc->obj_size, num_pages, slab->obj_num);
#endif
	return slab;
}

static addr_t do_alloc_object(slab_t *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	slab_cache_t *sc = (slab_cache_t *)(slab->parent);
	assert(sc);
	if(slab->obj_used >= slab->obj_num)
		panic(PANIC_MEM | PANIC_NOSYNC, 
				"Slab object count is greater than possible number of objects");
	assert(slab->stack > slab->stack_arr && slab->stack_arr);
	/* Pop the object off the top of the stack */
	unsigned short obj = *(--slab->stack);
	assert(obj != 0xFFFF);
	addr_t obj_addr = FIRST_OBJ(slab) + OBJ_SIZE(slab)*obj;
	assert((obj_addr > (addr_t)slab) && (((obj_addr+sc->obj_size)-(addr_t)slab) <= slab->num_pages*0x1000));
	slab->obj_used++;
	return obj_addr;
}

static addr_t alloc_object(slab_t *slab)
{
	addr_t ret = do_alloc_object(slab);
	assert(slab->obj_used <= slab->obj_num);
	if(ret && slab->obj_used == slab->obj_num)
	{
#ifdef SLAB_DEBUG
		printk(0, "%x: moving to FULL\n", (addr_t)slab);
#endif
		remove_slab_list(slab);
		add_slab_to_list(slab, TO_FULL);
	}
	else if(ret && slab->obj_used == 1)
	{
		remove_slab_list(slab);
		add_slab_to_list(slab, TO_PARTIAL);
	}
	return ret;
}

static unsigned do_release_object(slab_t *slab, int obj)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	assert(slab->obj_used);
	assert((unsigned)obj < slab->obj_num);
	/* Push the object onto the stack */
	*(slab->stack) = (unsigned short)obj;
	slab->stack++;
	unsigned ret = --slab->obj_used;
	return ret;
}

static void release_object(slab_t *slab, int obj)
{
	unsigned res = do_release_object(slab, obj);
	if(!res)
	{
		/* We must move it to the empty list or destroy it */
#ifdef SLAB_DEBUG
		printk(0, "%x: destroy\n", (addr_t)slab);
#endif
		remove_slab_list(slab);
		release_slab(slab);
	} else
	{
		/* If it was in the full, then we must switch lists */
		if((res+1) >= (slab->obj_num))
		{
			remove_slab_list(slab);
			add_slab_to_list(slab, TO_PARTIAL);
		}
	}
}

static void do_release_slab(slab_t *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	slab_cache_t *sc = (slab_cache_t *)(slab->parent);
	assert(sc);
	if(slab->obj_used)
		panic(PANIC_MEM | PANIC_NOSYNC, "Tried to release used slab");
	--sc->slab_count;
	remove_slab_list(slab);
	free_slab(slab);
}

static void release_slab(slab_t *slab)
{
	slab_cache_t *sc = (slab_cache_t *)(slab->parent);
	assert(sc);
	do_release_slab(slab);
	if(!sc->slab_count)
		release_scache(sc);
}

unsigned __mm_slab_init(addr_t start, addr_t end)
{
	printk(1, "[slab]: Initiating slab allocator...");
	map_if_not_mapped(start);
	map_if_not_mapped(start + 0x1000);
	slab_start = start;
	slab_end = end;
	assert(start < end && start);
	vmem_create(&slab_area_alloc, start+PAGE_SIZE, end, SLAB_NUM_INDEX);
	unsigned i;
	for(i=0;i<NUM_SCACHES;i++)
	{
		scache_list[i] = (slab_cache_t *)(start + sizeof(slab_cache_t)*i);
		scache_list[i]->id=-1;
	}
	printk(1, "done\n");
	pages_used = SLAB_NUM_INDEX+1;
	mutex_create(&scache_lock, 0);
	return 0;
}

/* Look through the available caches for: 
 * 	> Size matching
 * 	> Alignment (optional)
 * 	> Space available
 * 
 * If it can't find any slots without creating a new slab:
 * 	> If a cache of the right size already exists
 * 		> Add a slab to it
 * 	> Otherwise
 * 		> Do we have caches left?
 * 			(yes)> Create a new cache
 * 			(no) > Look for free slots of differing sizes
 */

static unsigned slab_size(int sz)
{
	unsigned s = (sz * MAX_OBJ_ID) + sizeof(slab_t);
	if(s > (0x1000 * 128))
		s = 0x1000 * 128;
	if(s < (unsigned)sz * 2) s = sz * 2;
	s = (s&PAGE_MASK) + 0x1000;
	return s;
}

static slab_t *find_usable_slab(unsigned size, int align, int allow_range)
{
	unsigned i;
	int perfect_fit=0;
	slab_cache_t *new_sc;
	slab_cache_t *sc=0;
	unsigned short fl=0;
	look_again_perfect:
	i = perfect_fit;
	if(i) i++;
	for(;i<NUM_SCACHES;i++)
	{
		/* If we want aligned objects, we need to have sizes 
		 * multiples of 0x1000 */
		if(scache_list[i]->id != -1 
			&& scache_list[i]->obj_size >= size 
			&& (scache_list[i]->obj_size <= size*RANGE_MUL || allow_range == 2)) 
		{
			if(scache_list[i]->obj_size != size && !allow_range)
				continue;
			if(!align && (scache_list[i]->flags & S_ALIGN))
				continue;
			if(align && !(scache_list[i]->flags & S_ALIGN))
				continue;
			perfect_fit = i;
			break;
		}
	}
	if(i == NUM_SCACHES)
		goto not_found;
	sc = scache_list[perfect_fit];
	assert(sc);
	if(sc->partial || sc->empty)
	{/* Good, theres room */
		slab_t *slab = sc->partial;
		if(align)
		{
			while(slab && !(slab->flags & S_ALIGN)) slab=(slab_t *)slab->next;
			if(!slab)
			{
				slab = sc->empty;
				while(slab && !(slab->flags & S_ALIGN)) slab=(slab_t *)slab->next;
			}
		} else
		{
			if(!slab) slab = sc->empty;
		}
		if(!slab)
		{
			/* Should we look for a different cache, or add a new slab? */
			if(sc->obj_size == size) {
				goto add_new_slab;
			}
			goto look_again_perfect;
		}
		
		return slab;
	} else
	{
		add_new_slab:
		fl=0;
		/* Can we add a slab to this cache? */
		/* Yes: Do it.
		 * 
		 * No: Look again
		 * 
		 */
		if(align) fl |= S_ALIGN;
		slab_t *slab = create_slab(sc, align ? slab_size(size)/PAGE_SIZE + 128 
				: slab_size(size)/PAGE_SIZE, fl);
		if(!slab)
		{
			tm_engage_idle();
			tm_delay(100);
			return 0;
		} else 
			add_slab_to_list(slab, TO_EMPTY);
		return slab;
	}
	not_found:
	/* Couldn't find a perfect fit.
	 * Either add a new cache, or use a close fit. */
	new_sc = get_empty_scache(size, align ? S_ALIGN : 0);
	if(!new_sc)
	{
		//if(allow_range)
		//	panic("Ran out of slab caches!");
		return find_usable_slab(size, align, 2);
	}
#ifdef SLAB_DEBUG
	printk(1, "[slab]: Allocated new slab cache @ %x (%d/%d)\n", (addr_t)new_sc, num_scache, NUM_SCACHES);
#endif
	return find_usable_slab(size, align, 0);
}

addr_t __mm_do_kmalloc_slab(size_t sz, char align)
{
	if(sz < 32) sz=32;
	if(!align)
		sz += (sizeof(addr_t) * 2);
	slab_t *slab=0, *old_slab=0;
	try_again:
	slab = find_usable_slab(sz, align, 1);
	if(!slab)
		goto try_again;
	assert(slab && slab->magic == SLAB_MAGIC);
	if(slab->obj_used >= slab->obj_num)
		panic(PANIC_MEM | PANIC_NOSYNC, 
			"Attemping to allocate from full slab: %d:%d\n", 
			slab->obj_used, slab->obj_num);
#ifdef SLAB_DEBUG
	total += ((slab_cache_t *)(slab->parent))->obj_size;
#endif
	addr_t addr = alloc_object(slab);
	if(align && !((addr&PAGE_MASK) == addr))
		panic(PANIC_MEM | PANIC_NOSYNC, 
			"slab allocation of aligned data failed! (returned %x)", addr);
	if(!align)
	{
		if(((addr + sizeof(addr_t *)) & PAGE_MASK) == (addr + sizeof(addr_t *)))
		{
			/* force non-alignment */
			addr += sizeof(addr_t);
		}
		*(addr_t *)(addr) = (addr_t)slab;
		addr += sizeof(addr_t);
	}
#ifdef SLAB_DEBUG
	printk(0, "A-> %d\n", total);
#endif
	if(!align)
		assert((addr & PAGE_MASK) != addr);
	return addr;
}

void __mm_do_kfree_slab(void *ptr)
{
	if(!((addr_t)ptr >= slab_start && (addr_t)ptr < slab_end))
	{
		panic(PANIC_NOSYNC, "kfree got invalid address %x, pid=%d, sys=%d\n", 
			ptr, current_task ? current_task->pid : 0, 
			current_task ? current_task->system : 0);
		return;
	}
	vnode_t *n = 0;
	slab_t *slab=0;
	if(((addr_t)ptr&PAGE_MASK) == (addr_t)ptr) {
		try_alt:
		n = vmem_find_node(&slab_area_alloc, (addr_t)ptr);
		if(n) slab = (slab_t *)n->addr;
	}
	else {
		slab = (slab_t *)*(addr_t *)((addr_t)ptr - sizeof(addr_t));
		n = slab->vnode;
		if(!n) goto try_alt;
 	}
	if(!n || !slab)
		panic(PANIC_MEM | PANIC_NOSYNC, "Kfree got invalid address in task %d, system=%d (%x)", current_task->pid, current_task->system, ptr);
	assert(slab->magic == SLAB_MAGIC);
	slab_cache_t *sc = (slab_cache_t *)slab->parent;
#ifdef SLAB_DEBUG
	total -= sc->obj_size;
	printk(0, "R-> %d\n", total);
#endif
	unsigned obj;
	obj = ((addr_t)ptr-FIRST_OBJ(slab)) / sc->obj_size;
	assert(obj < slab->obj_num);
	release_object(slab, obj);
}
