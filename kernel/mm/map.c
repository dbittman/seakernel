#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/fs/inode.h>
#include <sea/lib/linkedlist.h>
#include <sea/mm/map.h>

#include <sea/mm/valloc.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

static struct memmap *initialize_map(struct inode *node, 
			addr_t virt_start, int prot, int flags, size_t offset, size_t length)
{
	struct memmap *map = kmalloc(sizeof(struct memmap));
	map->node = node;
	map->prot = prot;
	map->flags = flags;
	map->offset = offset;
	map->length = length;
	map->virtual = virt_start;
	map->vr.start = 0;
	return map;
}

static int is_valid_location(addr_t addr)
{
	if(addr >= MEMMAP_USERSPACE_MAXIMUM || addr < MEMMAP_IMAGE_MINIMUM)
		return 0;
	return 1;
}

static int acquire_virtual_location(struct valloc_region *vr, addr_t *virt, int fixed, size_t length)
{
	assert(virt);
	vr->start=0;
	/* if the given address is valid, use that. */
	if(is_valid_location(*virt) && is_valid_location(*virt + length)) {
		return 0;
	} else if(fixed) {
		/* if it wasn't valid, and MAP_FIXED was specified, fail. */
		*virt = 0;
		return 0;
	}
	/* otherwise, allocate from given range... */
	int npages = ((length-1)/PAGE_SIZE) + 1;
	*virt = 0;
	vr->start = 0;
	valloc_allocate(&(current_process->mmf_valloc), vr, npages);
	return 1;
}

static void release_virtual_location(struct valloc_region *vr)
{
	if(!vr || !vr->start)
		return;
	valloc_deallocate(&(current_process->mmf_valloc), vr);
}

static void record_mapping(struct memmap *map)
{
	linkedlist_insert(&current_process->mappings, &map->entry, map);
}

static void remove_mapping(struct memmap *map)
{
	linkedlist_remove(&current_process->mappings, &map->entry);
}

static void disengage_mapping_region(struct memmap *map, addr_t start, size_t offset, size_t length)
{
	if((map->flags & MAP_SHARED)) {
		if(map->prot & PROT_WRITE) {
			fs_inode_sync_region(map->node, start, offset, length);
		}
		fs_inode_unmap_region(map->node, start, offset, length);
	} else {
		size_t o=0;
		/* we don't need to tell mminode about it, since it maps the page and the forgets
		 * about it */
		for(addr_t v = start;v < (start + length);v += PAGE_SIZE, o += PAGE_SIZE) {
			addr_t phys = mm_virtual_unmap(v);
			if(phys)
				mm_physical_decrement_count(phys);
		}
	}
}

static void disengage_mapping(struct memmap *map)
{
	disengage_mapping_region(map, map->virtual, map->offset, map->length);
}

addr_t mm_establish_mapping(struct inode *node, addr_t virt, 
		int prot, int flags, size_t offset, size_t length)
{
	assert(node);
	if(!(flags & MAP_ANONYMOUS) && (offset >= (size_t)node->length)) {
		return -EINVAL;
	}
	vfs_inode_get(node);
	mutex_acquire(&current_process->map_lock);
	/* get a virtual region to use */
	struct valloc_region vr;
	int ret = acquire_virtual_location(&vr, &virt, flags & MAP_FIXED, length);
	if(!ret && !virt) {
		mutex_release(&current_process->map_lock);
		vfs_icache_put(node);
		return -ENOMEM;
	}
	if(vr.start)
		virt = vr.start;
	//printk(0, "[mmap]: mapping %x for %x, f=%x, p=%x: %d:%x\n", 
	//		virt, length, flags, prot, node->id, offset);
	struct memmap *map = initialize_map(node, virt, prot, flags, offset, length);
	if(vr.start)
		memcpy(&(map->vr), &vr, sizeof(struct valloc_region));
	
	record_mapping(map);
	/* unmap the region of previous pages */
	for(addr_t s=virt;s < (virt + length);s+=PAGE_SIZE)
	{
		addr_t phys = mm_virtual_unmap(s);
		if(phys)
			mm_physical_decrement_count(phys);
	}
	/* if it's MAP_SHARED, then notify the mminode framework. Otherwise, we just
	 * wait for a pagefault to bring in the pages */
	if((flags & MAP_SHARED))
		fs_inode_map_region(map->node, map->offset, map->length);
	mutex_release(&current_process->map_lock);
	return virt;
}

static int __do_mm_disestablish_mapping(struct memmap *map)
{
	release_virtual_location(&map->vr);
	vfs_icache_put(map->node);
	map->node = 0;
	remove_mapping(map);
	kfree(map);
	return 0;
}

int mm_disestablish_mapping(struct memmap *map)
{
	mutex_acquire(&current_process->map_lock);
	disengage_mapping(map);
	int ret = __do_mm_disestablish_mapping(map);
	mutex_release(&current_process->map_lock);
	return ret;
}

int mm_sync_mapping(struct memmap *map, addr_t start, size_t length, int flags)
{
	if(!(map->flags & MAP_SHARED) || (map->flags & MAP_ANONYMOUS) || !(map->prot & PROT_WRITE))
		return 0;
	size_t fo = (start - map->virtual);
	for(addr_t v = 0;v < length;v += PAGE_SIZE) {
		if(mm_virtual_getmap(start + v, NULL, NULL)) {
			size_t page_len = PAGE_SIZE;
			if((length - v) < PAGE_SIZE)
				page_len = length - v;
			fs_inode_sync_physical_page(map->node, start + v, map->offset + fo + v, page_len);
		}
	}
	return 0;
}

void mm_mappings_clone(struct process *child)
{
	struct linkedentry *node;
	if(current_process->pid == 0)
		return;
	for(node = linkedlist_iter_start(&current_process->mappings);
			node != linkedlist_iter_end(&current_process->mappings);
			node = linkedlist_iter_next(node)) {
		struct memmap *map = linkedentry_obj(node);
		/* first create the new mapping structure */
		struct memmap *new = kmalloc(sizeof(*map));
		memcpy(new, map, sizeof(*new));
		vfs_inode_get(new->node);
		linkedlist_insert(&child->mappings, &new->entry, new);
		/* okay, now do the mapping */
		int attr = PAGE_PRESENT | PAGE_USER;
		if(new->flags & MAP_SHARED) {
			fs_inode_map_region(new->node, new->offset, new->length);
		}
		if((new->flags & MAP_PRIVATE) && (new->prot & PROT_WRITE)) {
			attr |= PAGE_COW;
		} else if(new->prot & PROT_WRITE) {
			attr |= PAGE_WRITE;
		}
		addr_t end = ((new->virtual + new->length - 1) & PAGE_MASK) + PAGE_SIZE;
		for(addr_t virt = new->virtual; virt < end; virt += PAGE_SIZE) {
			addr_t page;
			bool r = mm_virtual_getmap(virt, &page, NULL);
			if(r) {
				if(attr & PAGE_COW) {
					mm_virtual_changeattr(virt, attr, PAGE_SIZE);
				}
				mm_context_virtual_map(&child->vmm_context, virt, page, attr, PAGE_SIZE);
				mm_physical_increment_count(page);
			}
		}
		
	}
	memcpy(&child->mmf_valloc, &current_process->mmf_valloc, sizeof(child->mmf_valloc));
	mutex_create(&child->mmf_valloc.lock, 0);
}

/* linear scan over all mappings for an address...yeah, it's a but slow, but
 * most programs aren't going to have a large number of mapped regions...probably */
static struct memmap *find_mapping(addr_t address)
{
	struct linkedentry *n;
	struct memmap *map;
	/* don't worry about the list's lock, we already have a lock */
	for(n = linkedlist_iter_start(&current_process->mappings);
			n != linkedlist_iter_end(&current_process->mappings);
			n = linkedlist_iter_next(n)) {
		map = linkedentry_obj(n);
		/* check if address is in range. A special case to note: 'length' may not
		 * be page aligned, but here we act as if it were rounded up. 'length' refers
		 * to file length, but in a partial page, memory mapping is still valid for
		 * the rest of the page, just filled with zeros. */
		size_t rounded_length = map->length;
		if(rounded_length & (~PAGE_MASK))
			rounded_length = (rounded_length & PAGE_MASK) + PAGE_SIZE;
		if(address >= map->virtual && address < (map->virtual + rounded_length))
			return map;
	}
	return 0;
}

static int load_file_data(struct memmap *map, addr_t fault_address)
{
	/* whee, page alignment */
	addr_t address = fault_address & PAGE_MASK;
	addr_t diff = address - map->virtual;
	size_t offset = map->offset + diff;
	assert(!(offset & ~PAGE_MASK));
	assert(diff < map->length);
	int attr = ((map->prot & PROT_WRITE) ? PAGE_WRITE : 0);
	size_t page_len = PAGE_SIZE;
	if(map->length - diff < PAGE_SIZE)
		page_len = map->length - diff;
	if((map->flags & MAP_SHARED)) {
		fs_inode_map_shared_physical_page(map->node, address, offset, 
				FS_INODE_POPULATE, PAGE_PRESENT | PAGE_USER | attr);
	} else {
		fs_inode_map_private_physical_page(map->node, address, offset, PAGE_PRESENT | PAGE_USER | attr, page_len);
	}
	return 0;
}
#include <sea/vsprintf.h>
/* handles a pagefault. If we can find the mapping, we need to check
 * the cause of the fault against what we're allowed to do, and then
 * either fail, or load the data correctly. */
int mm_page_fault_test_mappings(addr_t address, int pf_cause)
{
	mutex_acquire(&current_process->map_lock);
	struct memmap *map = find_mapping(address);
	if(!map) {
		mutex_release(&current_process->map_lock);
		return -1;
	}
	/* check protections */
	int attr;
	addr_t page;
	if(mm_virtual_getmap(address, &page, &attr)) {
		if(attr & PAGE_COW) {
			size_t count = mm_physical_get_count(page);
			assert(count);
			if(count > 1) {
				addr_t newpage = mm_physical_allocate(PAGE_SIZE, false);
				mm_physical_increment_count(newpage);
				mm_physical_memcpy((void *)newpage, (void *)page, PAGE_SIZE, PHYS_MEMCPY_MODE_BOTH);
				mm_virtual_unmap(address & PAGE_MASK);
				mm_virtual_map(address & PAGE_MASK, newpage,
						PAGE_PRESENT | PAGE_USER | PAGE_WRITE, PAGE_SIZE);
				mm_physical_decrement_count(page);
			} else {
				mm_virtual_changeattr(address & PAGE_MASK, PAGE_PRESENT | PAGE_USER | PAGE_WRITE, PAGE_SIZE);
			}
			mutex_release(&current_process->map_lock);
			return 0;
		}
		/* Not sure why this happens... TODO */
		printk(0, "Mapped, but not COW! %x %x: %x (%x %x)\n",
				address, attr, pf_cause, map->flags, map->prot);
		mutex_release(&current_process->map_lock);
		return 0;
	}
	if((pf_cause & PF_CAUSE_READ) && !(map->prot & PROT_READ))
		goto out;
	if((pf_cause & PF_CAUSE_WRITE) && !(map->prot & PROT_WRITE))
		goto out;
	
	mutex_release(&current_process->map_lock);
	if(load_file_data(map, address) != -1)
	{
		return 0;
	}
	return -1;
out:
	mutex_release(&current_process->map_lock);
	return -1;
}

int mm_mapping_msync(addr_t start, size_t length, int flags)
{
	mutex_acquire(&current_process->map_lock);
	for(addr_t addr = start; addr < (start + length); addr += PAGE_SIZE)
	{
		struct memmap *map = find_mapping(addr);
		size_t page_len = PAGE_SIZE;
		if(length - (addr - start) < PAGE_SIZE)
			page_len = length - (addr - start);
		mm_sync_mapping(map, addr, page_len, flags);
	}
	mutex_release(&current_process->map_lock);
	return 0;
}

/* unmap page-by-page. This is quite slow compared to unmapping entire maps at once,
 * but it also makes for a much simpler implementation of munmaps crazy nonsense.*/
int mm_mapping_munmap(addr_t start, size_t length)
{
	/* TODO: support non-page-aligned length */
	mutex_acquire(&current_process->map_lock);
	for(addr_t addr = start; addr < (start + length); addr += PAGE_SIZE)
	{
		struct memmap *map = find_mapping(addr);
		if(!map)
			continue;
		mm_sync_mapping(map, addr, PAGE_SIZE, 0);
		size_t rounded_length = map->length;
		size_t page_len = PAGE_SIZE;
		if(rounded_length & ~(PAGE_MASK)) {
			rounded_length = (rounded_length & PAGE_MASK) + PAGE_SIZE;
			page_len = map->length & (~PAGE_MASK);
		}
		
		/* unmap this specific page */
		disengage_mapping_region(map, addr, map->offset + (addr - map->virtual), PAGE_SIZE);
		if(map->virtual == addr) {
			/* page is at the start of the map, so change the mapping */
			map->virtual += PAGE_SIZE;
			map->offset  += PAGE_SIZE;
			if(map->length < PAGE_SIZE)
				map->length = 0;
			else
				map->length -= PAGE_SIZE;
		} else if((map->virtual + (rounded_length - PAGE_SIZE)) == addr) {
			/* page is at the end of the map */
			map->length -= page_len;
		} else {
			/* the page splits the map. create a second mapping */
			struct memmap *n = initialize_map(map->node, map->virtual, map->prot, map->flags, map->offset, map->length);
			n->length -= ((addr + PAGE_SIZE) - n->virtual);
			n->offset += (addr - n->virtual) + PAGE_SIZE;
			n->virtual = addr + PAGE_SIZE;
			map->length = (addr - map->virtual);
			/* we don't need to notify the mminode framework that a new mapping has been created, since
			 * the counts on the pages haven't actually changed */
			if(map->vr.start) {
				/* split virtual node */
				valloc_split_region(&(current_process->mmf_valloc), 
						&map->vr, &n->vr, map->length / PAGE_SIZE);
			}
			/* remember to increase the count of the inode... */
			vfs_inode_get(n->node);
			record_mapping(n);
		}
		if(map->length == 0)
			__do_mm_disestablish_mapping(map);
	}
	mutex_release(&current_process->map_lock);
	return 0;
}

/* called by exit and exec */
void mm_destroy_all_mappings(struct process *t)
{
	/* we don't need to lock, because we assume only one thread */
	struct linkedentry *cur, *next;
	struct memmap *map;
	for(cur = linkedlist_iter_start(&t->mappings);
			cur != linkedlist_iter_end(&t->mappings);
			cur = next) {
		map = linkedentry_obj(cur);
		next = linkedlist_iter_next(cur);
		disengage_mapping(map);
		__do_mm_disestablish_mapping(map);
	}
	assert(t->mappings.count == 0);
}

