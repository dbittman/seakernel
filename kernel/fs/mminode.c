/* mminode.c: handle shared memory mapping of files at the inode level. The inode
 * keeps track of physical pages that get shared among mappings from different
 * processes. It handles loading data from pagefaulting, etc.
 */

#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/mm/vmm.h>
#include <stdatomic.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>
/* Each page of the inode has a count associated with it. When the count reaches
 * zero, the page is written to disk, and the page is freed. When region is mapped,
 * the pages aren't necessarily allocated right away. A call to map_region
 * is made, each page in the region has its count increased. The page is only allocated
 * after a pagefault. A call to unmap_region decreases the counts of all pages in the
 * region.
 */

struct physical_page {
	addr_t page;
	int count;
	mutex_t lock;
};

/* if physicals hasn't been initialized, initialize it. */
static void __init_physicals(struct inode *node)
{
	if(!node->physicals) {
		node->physicals = hash_table_create(0, 0, HASH_TYPE_CHAIN);
		hash_table_resize(node->physicals, HASH_RESIZE_MODE_IGNORE, 1000);
		hash_table_specify_function(node->physicals, HASH_FUNCTION_DEFAULT);
	}
}

addr_t fs_inode_map_private_physical_page(struct inode *node, addr_t virt,
		size_t offset, int attrib, size_t req_len)
{
	addr_t ph;
	assert(!(virt & ~PAGE_MASK));
	assert(!(offset & ~PAGE_MASK));
	/* specify MAP_ZERO, since read_inode may not fill up the whole page */
	ph = mm_physical_allocate(0x1000, false); //TODO: make sizes be req_len?
	bool result = mm_virtual_map(virt, ph, MAP_ZERO | attrib, 0x1000);
	if(!result)
		panic(0, "trying to remap mminode private section %x", virt);
	int err=-1;
	/* try to read the data. If this fails, we don't really have a good way 
	 * of telling userspace this...eh.
	 */
	size_t len = req_len;
	if(len + offset > (size_t)node->length)
		len = node->length - offset;
	if(offset < (size_t)node->length) {
		if(node->filesystem) {
			err = fs_inode_read(node, offset, len, (void *)virt);
			if(err < 0 || (size_t)err != len)
				printk(0, "[mminode]: read inode failed with %d\n", err);
		}
	}
	return ph;
}

/* try to map a physical page of an inode to a virtual address. If FS_INODE_POPULATE
 * is passed in flags, then if the page doesn't exist, then it allocates a physical
 * page, maps it, and loaded the data. If that flag is not passed, then it simply
 * tries to return the physical page.
 */
addr_t fs_inode_map_shared_physical_page(struct inode *node, addr_t virt, 
		size_t offset, int flags, int attrib)
{
	/* TODO: Handle non page-aligned lengths */
	assert(!(virt & ~PAGE_MASK));
	assert(!(offset & ~PAGE_MASK));
	/* test if we have any shared mappings... */
	if(!node->physicals)
		return 0;
	int page_number = offset / PAGE_SIZE;
	void *value;
	if(hash_table_get_entry(node->physicals, &page_number, sizeof(page_number), 1, &value)
			== -ENOENT)
		return 0;
	struct physical_page *entry = value;
	assert(entry->count);
	/* so, we don't have to worry about someone decreasing to count to zero while we're working, 
	   since a process never calls this function without being responsible for one of the counts. */
	mutex_acquire(&entry->lock);
	if(!entry->page && (flags & FS_INODE_POPULATE))
	{
		/* map a new page into virt, and load data into it */
		entry->page = mm_alloc_physical_page();
		/* specify ZERO, since read_inode may not fill up the whole page. Also,
		 * specify PAGE_LINK so that mm_vm_clone doesn't copy shared pages */
		if(!mm_virtual_map(virt, entry->page, MAP_ZERO | attrib | PAGE_LINK, 0x1000))
			panic(0, "trying to remap mminode shared section");

		int err=-1;
		/* try to read the data. If this fails, we don't really have a good way 
		 * of telling userspace this...eh.
		 */
		size_t len = PAGE_SIZE;
		if(len + offset > (size_t)node->length)
			len = node->length - offset;
		if(offset < (size_t)node->length) {
			if(node->filesystem && (err=fs_inode_read(node, offset, len, (void *)virt) < 0))
				printk(0, "[mminode]: read inode failed with %d\n", err);
		}
		atomic_fetch_add(&node->mapped_pages_count, 1);
	} else if(entry->page) {
		if(!mm_virtual_map(virt, entry->page, attrib | PAGE_LINK, 0x1000))
			panic(0, "trying to remap mminode shared section");
	}
	addr_t ret = entry->page;
	mutex_release(&entry->lock);
	return ret;
}

static struct physical_page *__create_entry (void)
{
	struct physical_page *p = kmalloc(sizeof(struct physical_page));
	mutex_create(&p->lock, 0);
	return p;
}

/* increase the counts of all requested pages by 1 */
void fs_inode_map_region(struct inode *node, size_t offset, size_t length)
{
	mutex_acquire(&node->mappings_lock);
	__init_physicals(node);
	assert(!(offset & ~PAGE_MASK));
	int page_number = offset / PAGE_SIZE;
	int npages = ((length-1) / PAGE_SIZE) + 1;
	for(int i=page_number;i<(page_number+npages);i++)
	{
		void *value;
		struct physical_page *entry;
		if(hash_table_get_entry(node->physicals, &i, sizeof(i), 1, &value) == -ENOENT)
		{
			/* create the entry, and add it */
			entry = __create_entry();
			hash_table_set_entry(node->physicals, &i, sizeof(i), 1, entry);
			atomic_fetch_add_explicit(&node->mapped_entries_count, 1, memory_order_relaxed);
		}
		else
			entry = value;

		/* bump the count... */
		atomic_fetch_add_explicit(&entry->count, 1, memory_order_relaxed);
		/* NOTE: we're not actually allocating or mapping anything here, really. All we're doing
		 * is indicating our intent to map a certain section, so we don't free pages. */
	}
	atomic_thread_fence(memory_order_acq_rel);
	mutex_release(&node->mappings_lock);
}

void fs_inode_sync_physical_page(struct inode *node, addr_t virt, size_t offset, size_t req_len)
{
	assert(!(offset & ~PAGE_MASK));
	assert(!(virt & ~PAGE_MASK));
	if(!mm_virtual_getmap(virt, NULL, NULL))
		return;
	/* again, no real good way to notify userspace of a failure */
	size_t len = req_len;
	if(len + offset > (size_t)node->length)
		len = node->length - offset;
	if(offset >= (size_t)node->length)
		return;
	if(node->filesystem && fs_inode_write(node, offset, len, (void *)virt) < 0)
		printk(0, "[mminode]: warning: failed to write back data\n");
}

/* decrease the count of each requested page by 1, and unmap it from the virtual address.
 * if the count reaches zero, sync the page, free it, and delete the entry to the hash table */
void fs_inode_unmap_region(struct inode *node, addr_t virt, size_t offset, size_t length)
{
	mutex_acquire(&node->mappings_lock);
	assert(node->physicals);
	assert(!(offset & ~PAGE_MASK));
	assert(!(virt & ~PAGE_MASK));
	int page_number = offset / PAGE_SIZE;
	int npages = ((length-1) / PAGE_SIZE) + 1;
	for(int i=page_number;i<(page_number+npages);i++)
	{
		void *value;
		struct physical_page *entry;
		if(hash_table_get_entry(node->physicals, &i, sizeof(i), 1, &value) == 0)
		{
			/* decrease the count. Because it's unlikely that a single file
			 * is going to be mmapped my a lot of processes, we can just free
			 * everything in good faith that it'll be a while before we need
			 * to a page again
			 */
			entry = value;
			mutex_acquire(&entry->lock);
			if(atomic_fetch_sub(&entry->count, 1) == 1)
			{
				/* count is now zero. write back data, 
				 * free the page, delete the entry, free the entry */
				size_t page_len = PAGE_SIZE;
				if(length - (i-page_number)*PAGE_SIZE < PAGE_SIZE)
					page_len = length - (i-page_number)*PAGE_SIZE;
				fs_inode_sync_physical_page(node, virt + (i - page_number)*PAGE_SIZE,
						i * PAGE_SIZE, page_len);
				addr_t p;
				bool ismapped = mm_virtual_getmap(virt + (i - page_number)*PAGE_SIZE, &p, NULL);
				assert(!ismapped || p == entry->page);
				if(entry->page)
					mm_free_physical_page(entry->page);
				entry->page = 0;
				mutex_destroy(&entry->lock);
				kfree(entry);
				hash_table_delete_entry(node->physicals, &i, sizeof(i), 1);
				atomic_fetch_sub(&node->mapped_entries_count, 1);
			} else
				mutex_release(&entry->lock);
		}
		/* we'll actually do the unmapping too */
		unsigned attr;
		if(mm_virtual_getmap(virt + (i - page_number)*PAGE_SIZE, NULL, &attr)) {
			assertmsg(attr & PAGE_LINK, "need page_link here %x", attr);
			mm_virtual_unmap(virt + (i - page_number)*PAGE_SIZE);
		}
	}
	mutex_release(&node->mappings_lock);
}

void fs_inode_destroy_physicals(struct inode *node)
{
	/* this can only be called from free_inode, so we don't need to worry about locks */
	if(!node->physicals)
		return;
	assert(!node->physicals->count);
	hash_table_destroy(node->physicals);
	mutex_destroy(&node->mappings_lock);
}

