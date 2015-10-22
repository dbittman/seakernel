#ifndef __SEA_FS_DIR_H
#define __SEA_FS_DIR_H

#include <sea/fs/inode.h>
struct inode;
#define DNAME_LEN 256
#define DIRENT_UNLINK 1
struct dirent {
	_Atomic int count;
	_Atomic int flags;
	rwlock_t lock;
	struct inode *parent;
	struct filesystem *filesystem;
	uint32_t ino;
	char name[DNAME_LEN];
	size_t namelen;
	struct queue_item lru_item;
	struct hashelem hash_elem;
};
enum
{
	DT_UNKNOWN = 0,
	DT_FIFO = 1,
	DT_CHR = 2,
	DT_DIR = 4,
	DT_BLK = 6,
	DT_REG = 8,
	DT_LNK = 10,
	DT_SOCK = 12,
	DT_WHT = 14
};
struct dirent_posix {
	unsigned int d_ino;
	unsigned int d_off;
	unsigned short int d_reclen;
	unsigned char d_type;
	char d_name[];
};


int sys_chdir(char *n, int fd);
int sys_getdents(int, struct dirent_posix *, unsigned int);
int sys_mkdir(const char *path, mode_t mode);
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen);
struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen);
struct inode *fs_dirent_readinode(struct dirent *dir, int);
struct dirent *vfs_dirent_create(struct inode *node);
int vfs_dirent_release(struct dirent *dir);
void vfs_dirent_destroy(struct dirent *dir);
void vfs_dirent_acquire(struct dirent *dir);
void vfs_dirent_init();
void fs_dirent_remove_lru(struct dirent *dir);
size_t fs_dirent_reclaim_lru();

#endif
