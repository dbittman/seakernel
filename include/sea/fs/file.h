#ifndef __SEA_FS_FILE_H
#define __SEA_FS_FILE_H

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

#include <types.h>

#define FILP_HASH_LEN 512

struct file {
	unsigned int flags, fd_flags, count;
	off_t pos;
	struct inode * inode;
};

struct file_ptr {
	unsigned int num, count;
	struct file *fi;
};


#endif
