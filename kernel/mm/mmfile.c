/* memory mapped files implementation.
 * primary files:
 *    - kernel/mm/mmfile.c
 *    - kernel/mm/map.c
 *    - kernel/fs/mminode.c
 * other files:
 *    - kernel/tm/exit.c
 *    - kernel/tm/fork.c
 *    - kernel/loader/exec.c
 *
 * There are three distinct parts of this system. Virtual address allocation,
 * inode memory mapping, and mapping recording.
 *
 * Each mmap region needs a virtual address space section. If a valid address is
 * passed to mmap, it uses it. MAP_FIXED then only serves to cause mmap to fail
 * if the address is invalid, since the default behavior is to use the program's
 * specified address. If an invalid address is specified, a section of the address
 * space in MMF_BEGIN and MMF_END is used. This is accomplished with the vmem code
 * (kernel/mm/area.c).
 *
 * Each mmap section has an inode backing it. For MAP_ANONYMOUS files, this inode
 * is fake - it is created duing the mmap call, and is destroyed once the mapping
 * is destroyed. For non anonymous mappings, this inode has it's count increased
 * on mmap, and has iput called on it when the mapping is destroyed. The mminode
 * frame work is responsible for allocating physical pages for sections of the
 * file. These can be shared between mappings with MAP_SHARED. If MAP_PRIVATE is
 * specified, the mminode framework just loads and maps private pages. Thus, all
 * the mminode framework only really deals with MAP_PRIVATE and MAP_SHARED, and
 * kind of ignores MAP_ANONYMOUS.
 *
 * The map recording framework keeps track of mappings made by processes. This is
 * for the most part 1 to 1 with mmap calls, except in the case of strange munmap
 * shenanigans. Since munmap can unmap partial regions of mapped memory, and can
 * thus split a mmap'd region into 2...if that happens, it creates two mappings
 * of the remaining mapped pages.
 */

#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/mm/map.h>
#include <sea/types.h>
#include <sea/fs/file.h>
#include <sea/cpu/atomic.h>
#include <sea/sys/fcntl.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>

addr_t mm_mmap(addr_t address, size_t length, int prot, int flags, int fd, size_t offset)
{
	struct inode *node;
	if(flags & MAP_ANONYMOUS) {
		/* create fake inode */
		node = vfs_inode_create();
	} else {
		struct file *f = fs_get_file_pointer(current_task, fd);
		if(!f)
			return -EBADF;
		/* check permissions */
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
	return mm_mapping_msync((addr_t)address, length, flags);
}

