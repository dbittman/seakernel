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
	if(is_valid_location(*virt)) {
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

static void disengage_mapping(struct memmap *map)
{
	if((map->flags & MAP_SHARED)) {
		fs_inode_unmap_region(map->node, map->virtual, map->offset, map->length);
	} else {
		size_t o=0;
		for(addr_t v = map->virtual;v < (map->virtual + map->length);v += PAGE_SIZE, o += PAGE_SIZE) {
			if(mm_vm_get_map(v, 0, 0)) 
				mm_vm_unmap(v, 0);
		}
	}
}

addr_t mm_establish_mapping(struct inode *node, addr_t virt, 
		int prot, int flags, size_t offset, size_t length)
{
	assert(node);
	mutex_acquire(&current_task->thread->map_lock);
	vnode_t *vn = acquire_virtual_location(&virt, flags & MAP_FIXED, length);
	if(!vn && !virt)
		return -ENOMEM;
	if(vn)
		virt = vn->addr;
	struct memmap *map = initialize_map(node, virt, prot, flags, offset, length);
	if(vn)
		map->vn = vn;
	add_atomic(&node->count, 1);
	record_mapping(map);
	if((flags & MAP_SHARED))
		fs_inode_map_region(map->node, map->offset, map->length);
	mutex_release(&current_task->thread->map_lock);
	return 0;
}

int mm_disestablish_mapping(struct memmap *map)
{	
	mutex_acquire(&current_task->thread->map_lock);
	disengage_mapping(map);
	release_virtual_location(map->vn);
	vfs_iput(map->node);
	map->node = 0;
	remove_mapping(map);
	kfree(map);
	mutex_release(&current_task->thread->map_lock);
	return 0;
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

int mm_page_fault_test_mappings(addr_t address)
{
	mutex_acquire(&current_task->thread->map_lock);
	struct memmap *map = find_mapping(address);
	if(!map) {
		mutex_release(&current_task->thread->map_lock);
		return -1;
	}

	if(load_file_data(map, address) != -1)
	{
		mutex_release(&current_task->thread->map_lock);
		return 0;
	}
	mutex_release(&current_task->thread->map_lock);
	return -1;
}

