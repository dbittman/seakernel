#ifndef __SEA_DM_BLOCKDEV_H
#define __SEA_DM_BLOCKDEV_H

#include <stdint.h>
#include <sys/types.h>
#include <sea/tm/kthread.h>
#include <sea/mutex.h>
#include <sea/lib/hash.h>

struct blockdev {
	uint64_t partbegin;
	size_t partlen;
	size_t blocksize;

	ssize_t (*rw)(int dir, struct inode *node, uint64_t start, uint8_t *buffer, size_t count); /* TODO: we should use io vectoring for this (struct ioreq? struct buffers? */
	struct kthread elevator;
	struct mutex cachelock;
	struct hash cache;
};

#endif

