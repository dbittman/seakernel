#ifndef __SEA_FS_FLOCK_H
#define __SEA_FS_FLOCK_H
#include <sea/fs/inode.h>
#include <sea/fs/file.h>
struct flock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_pos;
	struct flock *next, *prev;
};

struct eflock {
	short	l_type;		/* F_RDLCK, F_WRLCK, or F_UNLCK */
	short	l_whence;	/* flag to choose starting offset */
	long	l_start;	/* relative offset, in bytes */
	long	l_len;		/* length, in bytes; 0 means lock to EOF */
	short	l_pid;		/* returned with F_GETLK */
	short	l_xxx;		/* reserved for future use */
	long	l_rpid;		/* Remote process id wanting this lock */
	long	l_rsys;		/* Remote system id wanting this lock */
};

void vfs_init_inode_flocks(struct inode *i);
int fs_fcntl_setlk(struct file *file, long arg);
int fs_fcntl_getlk(struct file *file, long arg);
int fs_fcntl_setlkw(struct file *file, long arg);
void vfs_destroy_flocks(struct inode *f);


#endif
