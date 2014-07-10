#ifndef __SEA_MM_MAP_H
#define __SEA_MM_MAP_H

#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/ll.h>

struct memmap {
	addr_t virtual;
	size_t length, offset;
	struct inode *node;
	int flags, prot;
	struct llistnode *entry;
};

#warning "fix these values"
#define MAP_ANONYMOUS 1
#define MAP_FIXED 2

#endif

