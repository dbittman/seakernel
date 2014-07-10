#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/mm/vmm.h>
#include <sea/mm/vmem.h>
#include <sea/fs/inode.h>
#include <sea/ll.h>
#include <sea/mm/map.h>

static struct memmap *initialize_map(struct inode *node, 
			addr_t virt_start, int prot, int flags, size_t offset, size_t length)
{	
}

static addr_t acquire_virtual_location(addr_t virt, int fixed, size_t length)
{
}

static void release_virtual_location(addr_t virt, size_t length)
{
}

static void record_mapping(struct memmap *map)
{
	map->entry = ll_insert(&current_task->thread->mappings, map);
}

static void remove_mapping(struct memmap *map)
{
	ll_remove(&current_task->thread->mappings, map->entry);
}

static void engage_mapping(struct memmap *map)
{
}

static void disengage_mapping(struct memmap *map)
{
}

int mm_establish_mapping(struct inode *node, addr_t virt, 
		int prot, int flags, size_t offset, size_t length)
{
	mutex_acquire(&current_task->thread->map_lock);
	addr_t address = acquire_virtual_location(virt, flags & MAP_FIXED, length);
	struct memmap *map = initialize_map(node, address, prot, flags, offset, length);
	engage_mapping(map);
	record_mapping(map);
	mutex_release(&current_task->thread->map_lock);
}

int mm_disestablish_mapping(struct memmap *map)
{	
	mutex_acquire(&current_task->thread->map_lock);
	disengage_mapping(map);
	release_virtual_location(map->virtual, map->length);
	if(map->node)
		vfs_iput(map->node);
	map->node = 0;
	remove_mapping(map);
	kfree(map);
	mutex_release(&current_task->thread->map_lock);
	return 0;
}

static struct memmap *find_mapping(addr_t address)
{

}

static int load_file_data(struct inode *i, size_t file_offset, addr_t address_start)
{
	/* this function must also do page alignment */
}

int mm_page_fault_test_mappings(addr_t address)
{
	mutex_acquire(&current_task->thread->map_lock);
	struct memmap *map = find_mapping(address);

	if(map->flags & MAP_ANONYMOUS)
	{
		mutex_release(&current_task->thread->map_lock);
		return 0;
	}
	if(!load_file_data(map->node, map->offset + (address - map->virtual), address))
	{
		mutex_release(&current_task->thread->map_lock);
		return 0;
	}
	mutex_release(&current_task->thread->map_lock);
	return 1;
}

