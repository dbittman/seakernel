#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/mm/vmem.h>
#include <sea/fs/inode.h>
#include <sea/ll.h>
#include <sea/mm/map.h>
#include <sea/mm/vmem.h>
#include <sea/cpu/atomic.h>

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
	return map;
}

static int is_valid_location(addr_t addr)
{
	if(addr >= TOP_TASK_MEM_EXEC || addr < EXEC_MINIMUM)
		return 0;
	return 1;
}

static vnode_t *acquire_virtual_location(addr_t *virt, int fixed, size_t length)
{
	assert(virt);
	if(is_valid_location(*virt) && is_valid_location(*virt + length)) {
		return 0;
	} else if(fixed) {
		*virt = 0;
		return 0;
	}
	/* otherwise, allocate from given range... */
	int npages = ((length-1)/PAGE_SIZE) + 1;
	*virt = 0;
	vnode_t *node = vmem_insert_node(&current_task->thread->mmf_vmem, npages);
	return node;
}

static void release_virtual_location(vnode_t *node)
{
	if(!node)
		return;
	vmem_remove_node(&current_task->thread->mmf_vmem, node);
}

static void record_mapping(struct memmap *map)
{
	map->entry = ll_insert(&current_task->thread->mappings, map);
}

static void remove_mapping(struct memmap *map)
{
	ll_remove(&current_task->thread->mappings, map->entry);
}

static void disengage_mapping_region(struct memmap *map, addr_t start, size_t offset, size_t length)
{
	if((map->flags & MAP_SHARED)) {
		fs_inode_unmap_region(map->node, start, offset, length);
	} else {
		size_t o=0;
		for(addr_t v = start;v < (start + length);v += PAGE_SIZE, o += PAGE_SIZE) {
			if(mm_vm_get_map(v, 0, 0)) 
				mm_vm_unmap(v, 0);
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
	if(!(flags & MAP_ANONYMOUS) && (offset >= (size_t)node->len)) {
		return -EINVAL;
	}
	mutex_acquire(&current_task->thread->map_lock);
	vnode_t *vn = acquire_virtual_location(&virt, flags & MAP_FIXED, length);
	if(!vn && !virt) {
		return -ENOMEM;
	}
	if(vn)
		virt = vn->addr;
	printk(0, "[mmap]: mapping %x for %x, f=%x, p=%x: %s:%d\n", 
			virt, length, flags, prot, node->i_ops ? node->name : "(ANON)", offset);
	struct memmap *map = initialize_map(node, virt, prot, flags, offset, length);
	if(vn)
		map->vn = vn;
	add_atomic(&node->count, 1);
	record_mapping(map);

	for(addr_t s=virt;s < (virt + length);s+=PAGE_SIZE)
	{
		if(mm_vm_get_map(s, 0, 0))
			mm_vm_unmap(s, 0);
	}
	if((flags & MAP_SHARED))
		fs_inode_map_region(map->node, map->offset, map->length);
	mutex_release(&current_task->thread->map_lock);
	return virt;
}

static int __do_mm_disestablish_mapping(struct memmap *map)
{	
	release_virtual_location(map->vn);
	vfs_iput(map->node);
	map->node = 0;
	remove_mapping(map);
	kfree(map);
	return 0;
}

int mm_disestablish_mapping(struct memmap *map)
{
	mutex_acquire(&current_task->thread->map_lock);
	disengage_mapping(map);
	int ret = __do_mm_disestablish_mapping(map);
	mutex_release(&current_task->thread->map_lock);
	return ret;
}

int mm_sync_mapping(struct memmap *map, addr_t start, size_t length, int flags)
{
	if(!(map->flags & MAP_SHARED) || (map->flags & MAP_ANONYMOUS))
		return 0;
	size_t fo = (start - map->virtual);
	for(addr_t v = 0;v < length;v += PAGE_SIZE) {
		if(mm_vm_get_map(start + v, 0, 0))
			fs_inode_sync_physical_page(map->node, start + v, map->offset + fo + v);
	}
	return 0;
}

static struct memmap *find_mapping(addr_t address)
{
	assert(mutex_is_locked(&current_task->thread->map_lock));
	struct llistnode *n;
	struct memmap *map;
	/* don't worry about the list's lock, we already have a lock */
	ll_for_each_entry(&current_task->thread->mappings, n, struct memmap *, map) {
		/* check if address is in range */
		if(address >= map->virtual && address < (map->virtual + map->length))
			return map;
	}
	return 0;
}

static int load_file_data(struct memmap *map, addr_t fault_address)
{
	/* whee, page alignment */
	addr_t address = fault_address & PAGE_MASK;
	size_t offset = map->offset + (address - map->virtual);
	assert(!(offset & ~PAGE_MASK));
	int attr = ((map->prot & PROT_WRITE) ? PAGE_WRITE : 0);
	if(map->flags & MAP_SHARED) {
		fs_inode_map_shared_physical_page(map->node, address, offset, FS_INODE_POPULATE, PAGE_PRESENT | PAGE_USER | attr);
	} else {
		fs_inode_map_private_physical_page(map->node, address, offset, PAGE_PRESENT | PAGE_USER | attr);
	}
	return 0;
}

int mm_page_fault_test_mappings(addr_t address, int pf_cause)
{
	mutex_acquire(&current_task->thread->map_lock);
	struct memmap *map = find_mapping(address);
	if(!map) {
		mutex_release(&current_task->thread->map_lock);
		return -1;
	}
	/* check protections */
	if((pf_cause & PF_CAUSE_READ) && !(map->prot & PROT_READ))
		goto out;
	if((pf_cause & PF_CAUSE_WRITE) && !(map->prot & PROT_WRITE))
		goto out;
	
	if(load_file_data(map, address) != -1)
	{
		mutex_release(&current_task->thread->map_lock);
		return 0;
	}
out:
	mutex_release(&current_task->thread->map_lock);
	return -1;
}

int mm_mapping_msync(addr_t start, size_t length, int flags)
{
	mutex_acquire(&current_task->thread->map_lock);
	for(addr_t addr = start; addr < (start + length); addr += PAGE_SIZE)
	{
		struct memmap *map = find_mapping(addr);
		mm_sync_mapping(map, addr, PAGE_SIZE, flags);
	}
	mutex_release(&current_task->thread->map_lock);
	return 0;
}

int mm_mapping_munmap(addr_t start, size_t length)
{
	mutex_acquire(&current_task->thread->map_lock);
	for(addr_t addr = start; addr < (start + length); addr += PAGE_SIZE)
	{
		struct memmap *map = find_mapping(addr);
		if(!map)
			continue;
		mm_sync_mapping(map, addr, PAGE_SIZE, 0);
		disengage_mapping_region(map, addr, map->offset + (addr - map->virtual), PAGE_SIZE);
		if(map->virtual == addr) {
			/* page is at the start of the map */
			map->virtual += PAGE_SIZE;
			map->offset  += PAGE_SIZE;
			map->length  -= PAGE_SIZE;
		} else if((map->virtual + (map->length - PAGE_SIZE)) == addr) {
			/* page is at the end of the map */
			map->length -= PAGE_SIZE;
		} else {
			/* the page splits the map */
			struct memmap *n = initialize_map(map->node, map->virtual, map->prot, map->flags, map->offset, map->length);
			n->length -= ((addr + PAGE_SIZE) - n->virtual);
			n->offset += (addr - n->virtual) + PAGE_SIZE;
			n->virtual = addr + PAGE_SIZE;
			map->length = (addr - map->virtual);
			if(map->vn) {
				/* split virtual node */
				n->vn = vmem_split_node(&current_task->thread->mmf_vmem, map->vn, map->length / PAGE_SIZE);
			}
			add_atomic(&n->node->count, 1);
			record_mapping(n);
		}
		if(map->length == 0)
			__do_mm_disestablish_mapping(map);
	}
	mutex_release(&current_task->thread->map_lock);
	return 0;
}

/* called by exit and exec */
void mm_destroy_all_mappings(task_t *t)
{
	mutex_acquire(&current_task->thread->map_lock);
	struct llistnode *cur, *next;
	struct memmap *map;
	ll_for_each_entry_safe(&(t->thread->mappings), cur, next, struct memmap *, map) {
		disengage_mapping(map);
		__do_mm_disestablish_mapping(map);
	}
	assert(t->thread->mappings.num == 0);
	mutex_release(&current_task->thread->map_lock);
}

