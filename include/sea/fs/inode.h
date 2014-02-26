#ifndef __SEA_FS_INODE_H
#define __SEA_FS_INODE_H

#include <types.h>
#include <mutex.h>
#include <rwlock.h>
#include <ll.h>
#include <sea/fs/flock.h>
#include <sea/fs/stat.h>
#include <sea/dm/pipe.h>

#define MAY_EXEC      0100
#define MAY_WRITE     0200
#define MAY_READ      0400



#define INAME_LEN 256

#define inode_has_children(i) (i->children.head && ll_is_active((&i->children)))

typedef struct {
	struct inode *root;
	struct inode *parent;
} mount_pt_t;

struct inode {
	/* Attributes */
	mode_t mode;
	uid_t uid;
	gid_t gid;
	unsigned short nlink;
	unsigned char dynamic, marked_for_deletion;
	unsigned int flags;
	off_t len;
	addr_t start;
	unsigned int nblocks;
	time_t ctime, atime, mtime;
	int count, f_count, newlocks;
	size_t blksize;
	/* Identification */
	char name[INAME_LEN];
	dev_t dev;
	unsigned long num;
	unsigned int sb_idx;
	char node_str[INAME_LEN];
	/* Pointers */
	struct inode_operations *i_ops;
	struct inode *parent;
	struct llist children;
	struct llistnode *node;
	struct inode *mount_parent;
	pipe_t *pipe;
	mount_pt_t *mount;
	/* Locking */
	rwlock_t rwl;
	struct flock *flocks;
	mutex_t *flm;
};

struct inode_operations {
	int (*read) (struct inode *, off_t, size_t, char *);
	int (*write) (struct inode *, off_t, size_t, char *);
	int (*select) (struct inode *, unsigned int);
	struct inode *(*create) (struct inode *,char *, mode_t);
	struct inode *(*lookup) (struct inode *,char *);
	struct inode *(*readdir) (struct inode *, unsigned);
	int (*link) (struct inode *, char *);
	int (*unlink) (struct inode *);
	int (*rmdir) (struct inode *);
	int (*sync_inode) (struct inode *);
	int (*unmount)(struct inode *, unsigned int);
	int (*fsstat)(struct inode *, struct posix_statfs *);
	int (*fssync)(struct inode *);
	int (*update)(struct inode *);
};

#define iremove_recur(i)  do_iremove(i, 2, 0)
#define iremove(i)        do_iremove(i, 0, 0)
#define iremove_nofree(i) do_iremove(i, 3, 0)
#define iremove_force(i)  do_iremove(i, 1, 0)

#define get_idir(path,in_st)         do_get_idir(path, in_st, 0, 0, 0)
#define lget_idir(path,in_st)        do_get_idir(path, in_st, 1, 0, 0)
#define clget_idir(path,in_st,x)     do_get_idir(path, in_st, 1, x, 0)
#define cget_idir(path,in_st,x)      do_get_idir(path, in_st, 1, x, 0)
#define ctget_idir(path,in_st,x,res) do_get_idir(path, in_st, 1, x, res)

#define add_inode(a,b) do_add_inode(a, b, 0)


#endif
