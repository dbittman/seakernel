#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/mm/map.h>
#include <sea/types.h>
#include <sea/fs/file.h>
#include <sea/cpu/atomic.h>
#include <sea/sys/fcntl.h>

addr_t mm_mmap(addr_t address, size_t length, int prot, int flags, int fd, size_t offset)
{
	struct inode *node;
	/* round up length */
	if(length & ~PAGE_MASK)
		length = (length & PAGE_MASK) + PAGE_SIZE;
	if(flags & MAP_ANONYMOUS) {
		/* create fake inode */
		node = kmalloc(sizeof(*node));
		mutex_create(&node->mappings_lock, 0);
		rwlock_create(&node->rwl);
		node->count = 1;
	} else {
		struct file *f = fs_get_file_pointer(current_task, fd);
		if(!f)
			return -EBADF;
		if(!(f->flags & _FREAD)) {
			fs_fput(current_task, fd, 0);
			return -EACCES;
		}
		if(!(flags & MAP_PRIVATE) && (prot & PROT_WRITE) && !(f->flags & _FWRITE)) {
			fs_fput(current_task, fd, 0);
			return -EACCES;
		}
		if(!S_ISREG(f->inode->mode)) {
			fs_fput(current_task, fd, 0);
			return -ENODEV;
		}
		node = f->inode;
		add_atomic(&f->inode->count, 1);
		fs_fput(current_task, fd, 0);
	}
	/* a mapping replaces any other mapping that it overwrites, according to opengroup */
	mm_mapping_munmap(address, length);
	addr_t mapped_address = mm_establish_mapping(node, address, prot, flags, offset, length);
	vfs_iput(node);
	return mapped_address;
}

void *sys_mmap(void *address, struct __mmap_args *args)
{
	if(((addr_t)address & ~PAGE_MASK))
		return (void *)-EINVAL;
	return (void *)mm_mmap((addr_t)address, args->length, args->prot, args->flags, args->fd, (size_t)args->offset);
}

int sys_munmap(void *addr, size_t length)
{
	if(((addr_t)addr & ~PAGE_MASK))
		return -EINVAL;
	/* round up length */
	if(length & ~PAGE_MASK)
		length = (length & PAGE_MASK) + PAGE_SIZE;
	return mm_mapping_munmap((addr_t)addr, length);
}

int sys_msync(void *address, size_t length, int flags)
{
	if(((addr_t)address & ~PAGE_MASK))
		return -EINVAL;
	/* round up length */
	if(length & ~PAGE_MASK)
		length = (length & PAGE_MASK) + PAGE_SIZE;
	return mm_mapping_msync((addr_t)address, length, flags);
}

