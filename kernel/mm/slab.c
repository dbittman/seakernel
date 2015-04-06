/* slab.c (kernel/mm): Implementation on a SLAB allocation system.
 * Implementation is Copyright (c) 2010 Daniel Bittman. 
 * 
 * Diagram of memory layout:
 * | = page bound
 * start                      --Slabs--->                      end
 * {----|----|----|----|----|----|----|----|----|----|----|----}
 *   ^--slab cache list
 *         ^---^----^----^----valloc allocation index
 * 
 */ 
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/cpu/atomic.h>
#include <sea/mm/slab.h>
#include <sea/mm/valloc.h>
#include <sea/vsprintf.h>

static addr_t slab_start=0, slab_end=0;
static struct valloc slab_valloc;

static struct slab_cache *scache_list[NUM_SCACHES];
static unsigned pages_used=0;

static unsigned num_slab=0, num_scache=0;
static void release_slab(struct slab *slab);
static mutex_t scache_lock;

int kerfs_kmalloc_report(size_t offset, size_t length, char *buf)
{
	size_t dl = 0;
	char tmp[10000];
	dl = snprintf(tmp, 100, "Pages Used: %d, Slab Count: %d, Scache Count: %d\n",
			pages_used, num_slab,
			num_scache);
	if(offset > dl)
		return 0;
	if(offset + length > dl)
		length = dl - offset;
	memcpy(buf, tmp + offset, length);
	return length;
}

static struct valloc_region *alloc_slab(struct valloc_region *vr, unsigned np)
{
	assert(np && vr);
	if(!valloc_allocate(&slab_valloc, vr, np))
		return 0;
	pages_used += np;
	add_atomic(&num_slab, 1);
	return vr;
}

static void free_slab(struct slab *slab)
{
	unsigned num_pages = slab->num_pages;
	assert(slab->magic == SLAB_MAGIC);
	assert(slab);
	pages_used -= slab->num_pages;
	slab->magic = 0;
	valloc_deallocate(&slab_valloc, &slab->vreg);
	addr_t j;
	addr_t addr = (addr_t)slab;
	for(j=addr;j<(addr + num_pages*PAGE_SIZE);j+=PAGE_SIZE) {
		if(mm_vm_get_map(j, 0, 0))
			mm_vm_unmap(j, 0);
	}
	sub_atomic(&num_slab, 1);
}

static struct slab_cache *get_empty_scache(int size)
{
	assert(size);
	unsigned i=0;
	mutex_acquire(&scache_lock);
	while(i < NUM_SCACHES)
	{
		if(scache_list[i]->id == -1)
			break;
		i++;
	}
	if(i == NUM_SCACHES) {
		/* couldn't find one */
		mutex_release(&scache_lock);
		return 0;
	}
	memset(scache_list[i], 0, sizeof(struct slab_cache));
	scache_list[i]->id = i;
	scache_list[i]->obj_size = size;
	add_atomic(&num_scache, 1);
	mutex_release(&scache_lock);
	return scache_list[i];
}

static void release_scache(struct slab_cache *sc)
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
static int add_slab_to_list(struct slab *slab, char to)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	assert(slab->next==0 && slab->prev==0);
	struct slab **list=0;
	struct slab_cache *sc = slab->parent;
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
	/* Woah, double pointers! While annoying, they make 
	 * things _much_ easier to handle. */
	struct slab *old = *list;
	*list = slab;
	if(old)
		old->prev = slab;
	slab->next = old;
	slab->prev=0;
	return 0;
}

static void remove_slab_list(struct slab *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	struct slab_cache *sc = slab->parent;
	assert(sc);
	if(slab->prev)
		slab->prev->next=slab->next;
	if(slab->next)
		slab->next->prev = slab->prev;
	/* If the lists of the cache point to this slab, we need to clear that... */
	struct slab **list=0;
	if(sc->empty == slab)
		list = &sc->empty;
	else if(sc->partial == slab)
		list = &sc->partial;
	else if(sc->full == slab)
		list = &sc->full;
	if(list)
		*list = (struct slab *)slab->next;
	slab->prev = slab->next=0;
}

static struct slab *create_slab(struct slab_cache *sc, int num_pages)
{
	assert(sc && num_pages);
	addr_t addr=0;
	struct valloc_region vreg;
	if(!alloc_slab(&vreg, num_pages))
		panic(PANIC_MEM | PANIC_NOSYNC, "create_slab: Unable to allocate slab");
	addr = vreg.start;
	sc->slab_count++;
	unsigned i;
	addr_t j;
	for(j=addr;j<(addr + num_pages*PAGE_SIZE);j+=PAGE_SIZE) {
		if(!mm_vm_get_map(j, 0, 0))
			mm_vm_map(j, mm_alloc_physical_page(), PAGE_PRESENT, MAP_CRIT);
	}
	struct slab *slab = (struct slab *)addr;
	assert(slab->magic != SLAB_MAGIC);
	memset(slab, 0, sizeof(struct slab));
	slab->magic = SLAB_MAGIC;
	slab->parent = sc;
	slab->num_pages = num_pages;
	memcpy(&slab->vreg, &vreg, sizeof(struct valloc_region));
	slab->obj_num = ((num_pages*PAGE_SIZE)-sizeof(struct slab)) / sc->obj_size;
	assert(slab->obj_num > 0);
	assert((int)sizeof(struct slab) < (num_pages * PAGE_SIZE));
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
		*tmp = 0xFFFF;
		i++;
		tmp++;
	}
	return slab;
}

static addr_t do_alloc_object(struct slab *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	struct slab_cache *sc = slab->parent;
	assert(sc);
	if(slab->obj_used >= slab->obj_num)
		panic(PANIC_MEM | PANIC_NOSYNC, 
				"Slab object count is greater than possible number of objects");
	assert(slab->stack > slab->stack_arr && slab->stack_arr);
	/* Pop the object off the top of the stack */
	unsigned short obj = *(--slab->stack);
	assert(obj != 0xFFFF);
	addr_t obj_addr = FIRST_OBJ(slab) + OBJ_SIZE(slab)*obj;
	assert((obj_addr > (addr_t)slab)
			&& (((obj_addr+sc->obj_size)-(addr_t)slab) <= slab->num_pages*PAGE_SIZE));
	slab->obj_used++;
	return obj_addr;
}

static addr_t alloc_object(struct slab *slab)
{
	addr_t ret = do_alloc_object(slab);
	assert(slab->obj_used <= slab->obj_num);
	if(ret && slab->obj_used == slab->obj_num)
	{
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

static unsigned do_release_object(struct slab *slab, int obj)
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

static void release_object(struct slab *slab, int obj)
{
	unsigned res = do_release_object(slab, obj);
	if(!res)
	{
		/* We must move it to the empty list or destroy it */
		remove_slab_list(slab);
		release_slab(slab);
	} else if((res+1) >= (slab->obj_num)) {
		/* If it was in the full, then we must switch lists */
		remove_slab_list(slab);
		add_slab_to_list(slab, TO_PARTIAL);
	}
}

static void do_release_slab(struct slab *slab)
{
	assert(slab && slab->magic == SLAB_MAGIC);
	struct slab_cache *sc = slab->parent;
	assert(sc);
	if(slab->obj_used)
		panic(PANIC_MEM | PANIC_NOSYNC, "Tried to release used slab");
	--sc->slab_count;
	remove_slab_list(slab);
	free_slab(slab);
}

static void release_slab(struct slab *slab)
{
	struct slab_cache *sc = slab->parent;
	assert(sc);
	do_release_slab(slab);
	if(!sc->slab_count)
		release_scache(sc);
}

unsigned __mm_slab_init(addr_t start, addr_t end)
{
	printk(1, "[slab]: Initiating slab allocator...");
	map_if_not_mapped(start);
	map_if_not_mapped(start + PAGE_SIZE);
	slab_start = start;
	slab_end = end;
	assert(start < end && start);
	valloc_create(&slab_valloc, start + PAGE_SIZE, end, PAGE_SIZE, 0);
	unsigned i;
	for(i=0;i<NUM_SCACHES;i++)
	{
		scache_list[i] = (struct slab_cache *)(start + sizeof(struct slab_cache)*i);
		scache_list[i]->id=-1;
	}
	printk(1, "done\n");
	pages_used = slab_valloc.nindex+1;
	mutex_create(&scache_lock, 0);
	return 0;
}

/* Look through the available caches for: 
 * 	> Size matching
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
	unsigned s = (sz * MAX_OBJ_ID) + sizeof(struct slab);
	if(s > (PAGE_SIZE * 128))
		s = PAGE_SIZE * 128;
	if(s < ((unsigned)sz + sizeof(struct slab)))
		s = sz + sizeof(struct slab);
	s = (s&PAGE_MASK) + PAGE_SIZE;
	return s;
}

static int __does_fit(size_t obj, size_t container, int any_fit)
{
	/* we allow a little bit of wasted space */
	if(container < obj)
		return 0;
	if(!any_fit && container > obj * 2)
		return 0;
	return 1;
}

static struct slab *find_usable_slab(size_t size, int any_fit)
{
	struct slab_cache *sc = 0;
	struct slab *slab;
	int i;
	/* search the existing slab caches for one that can hold this
	 * object reasonably */
	for(i=0;i<(int)NUM_SCACHES;i++) {
		if(scache_list[i]->id != -1
				&& __does_fit(size, scache_list[i]->obj_size, any_fit)) {
			sc = scache_list[i];
			break;
		}
	}
	/* not found, create a new one */
	if(i == NUM_SCACHES)
		sc = get_empty_scache(size);
	
	if(!sc) {
		/* we already tried everything we could do, so just fail */
		if(any_fit)
			return 0;
		/* we weren't even able to create a new slab cache, so try to
		 * fit this object in anywhere */
		return find_usable_slab(size, 1);
	}
	
	/* try to find a usable slab */
	slab = sc->partial ? sc->partial : sc->empty;
	if(!slab) {
		/* nothing in the partial or empty list, so create a slab and
		 * add use it */
		slab = create_slab(sc, slab_size(sc->obj_size) / PAGE_SIZE);
		if(!slab)
			panic(PANIC_MEM | PANIC_NOSYNC, "failed to add slab to scache");
		add_slab_to_list(slab, TO_EMPTY);
	}
	return slab;
}

/* handle aligned allocations as if each is its own slab. This
 * makes management if both aligned and non-aligned objects in
 * the same kmalloc region much easier */
addr_t __slab_do_kmalloc_aligned(size_t sz)
{
	struct valloc_region vr;
	assert(sz == PAGE_SIZE);
	valloc_allocate(&slab_valloc, &vr, sz / PAGE_SIZE);
	assert(!(vr.start & ~PAGE_MASK));
	/* we only need to map one page, since page-aligned allocations are only
	 * a single page size */
	map_if_not_mapped(vr.start);
	return vr.start;
}

void __slab_do_kfree_aligned(addr_t a)
{
	struct valloc_region vr;
	vr.start = a;
	vr.npages = 1;
	vr.flags = 0;
	mm_vm_unmap(a, 0);
	valloc_deallocate(&slab_valloc, &vr);
}

/* aligned allocations are handled seperately. Non-aligned ones
 * are forced to be non-aligned, since free checks whether the
 * pointer passed to it is aligned and handles that differently.
 *
 * So, many processors dislike accessing data that isn't word
 * aligned, so we just assume that that is always true (because
 * assumptions are like, totally great). We have to force
 * non-page-alignment to not screw over _mm_do_kfree_slab, so
 * we do that by (maybe) adding a sizeof(addr_t *), which is
 * (at least) word aligned.
 */
addr_t __mm_do_kmalloc_slab(size_t sz, char align)
{
	if(align)
		return __slab_do_kmalloc_aligned(sz);
	/* fix size. minimum size is 32 bytes, and we
	 * need a few extra bits for the non-alignment
	 * forcing */
	if(sz < 32)
		sz = 32;
	sz += (sizeof(addr_t) * 2);
	struct slab *slab = find_usable_slab(sz, 0);
	assert(slab && slab->magic == SLAB_MAGIC);
	if(slab->obj_used >= slab->obj_num)
		panic(PANIC_MEM | PANIC_NOSYNC, 
			"Attemping to allocate from full slab: %d:%d\n", 
			slab->obj_used, slab->obj_num);
	addr_t addr = alloc_object(slab);
	/* force non-page-alignment */
	if(((addr + sizeof(addr_t *)) & PAGE_MASK) == (addr + sizeof(addr_t *)))
		addr += sizeof(addr_t *);
	/* save a pointer to the slab that this is from for easy recovery in free */
	*(addr_t *)(addr) = (addr_t)slab;
	addr += sizeof(addr_t *);
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
	struct slab *slab=0;
	if(((addr_t)ptr&PAGE_MASK) == (addr_t)ptr) {
		/* the pointer is page aligned. handle it specially */
		__slab_do_kfree_aligned((addr_t)ptr);
		return;
	}
	else {
		slab = (struct slab *)*(addr_t *)((addr_t)ptr - sizeof(addr_t));
 	}
	assert(slab && slab->magic == SLAB_MAGIC);
	struct slab_cache *sc = slab->parent;
	unsigned obj;
	obj = ((addr_t)ptr-FIRST_OBJ(slab)) / sc->obj_size;
	assert(obj < slab->obj_num);
	release_object(slab, obj);
}

