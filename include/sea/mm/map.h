#ifndef __SEA_MM_MAP_H
#define __SEA_MM_MAP_H

#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/ll.h>
#include <sea/mm/vmem.h>

struct memmap {
	addr_t virtual;
	size_t length, offset;
	struct inode *node;
	int flags, prot;
	struct llistnode *entry;
	vnode_t *vn;
};

struct __mmap_args {
	size_t length;
	int prot;
	int flags;
	int fd;
	size_t offset;
};

/*
 *  * Prots to 'mmap'.
 *   */
#define PROT_READ       0x1
#define PROT_WRITE      0x2
#define PROT_EXEC       0x4
#define PROT_NONE       0x0
/*
 *  * Flags to 'mmap'.
 *   */
#define MAP_SHARED      0x001
#define MAP_PRIVATE     0x002
#define MAP_FIXED       0x010
#define MAP_FILE        0x000
#define MAP_ANONYMOUS   0x020
#define MAP_ANON        MAP_ANONYMOUS
#define MAP_GROWSDOWN   0x0100
#define MAP_DENYWRITE   0x0800
#define MAP_EXECUTABLE  0x1000
#define MAP_LOCKED      0x0080
#define MAP_NORESERVE   0x0040
/*
 *  * Failed flag from 'mmap'.
 *   */
#define MAP_FAILED_EADDR  ((unsigned long long) (-1LL))
/*
 *  * Flags to 'mremap'.
 *   */
#define MREMAP_MAYMOVE  1
/*
 *  * Flags to 'msync'.
 *   */
#define MS_ASYNC        1
#define MS_SYNC         4
#define MS_INVALIDATE   2

addr_t mm_establish_mapping(struct inode *node, addr_t virt, 
		int prot, int flags, size_t offset, size_t length);
int mm_disestablish_mapping(struct memmap *map);
int mm_sync_mapping(struct memmap *map, addr_t start, size_t length, int flags);
int mm_page_fault_test_mappings(addr_t address);
int mm_mapping_munmap(addr_t start, size_t length);
int mm_mapping_msync(addr_t start, size_t length, int flags);

int sys_msync(void *address, size_t length, int flags);
int sys_munmap(void *addr, size_t length);
void *sys_mmap(void *address, struct __mmap_args *args);
addr_t mm_mmap(addr_t address, size_t length, int prot, int flags, int fd, size_t offset);
#endif

