#ifndef __SEA_FS_INODE_H
#define __SEA_FS_INODE_H

#include <sea/types.h>
#include <sea/mutex.h>
#include <sea/rwlock.h>
#include <sea/ll.h>
#include <sea/fs/flock.h>
#include <sea/fs/stat.h>
#include <sea/lib/hash.h>
#include <sea/lib/queue.h>
#include <sea/fs/fs.h>
#define MAY_EXEC      0100
#define MAY_WRITE     0200
#define MAY_READ      0400

#define NAME_LEN 256
#define DNAME_LEN 256
typedef struct {
	struct inode *root;
	struct inode *parent;
} mount_pt_t;

typedef struct pipe_struct pipe_t;

struct inode;
struct dirent {
	int count;
	rwlock_t lock;
	struct inode *parent;
	struct filesystem *filesystem;
	uint32_t ino;
	char name[DNAME_LEN];
	size_t namelen;
};

#define INODE_NEEDREAD 1
#define INODE_DIRTY    2
#define INODE_INUSE    4

#define RESOLVE_NOLINK 1

struct inode {
	int count;
	rwlock_t lock;
	uint32_t flags;
	struct queue_item lru_item;
	struct llistnode inuse_item;
	
	mode_t mode;
	uid_t uid;
	gid_t gid;
	uint16_t nlink;
	size_t length;
	time_t ctime, atime, mtime;
	size_t blocksize;
	size_t nblocks;
	size_t id;

	struct hash_table *dirents;

	dev_t phys_dev;

	struct filesystem *filesystem;
	struct inode_operations *i_ops;
	
	mount_pt_t *mount;
	pipe_t *pipe;

	struct flock *flocks;
	mutex_t *flm;
	
	struct hash_table *physicals;
	mutex_t mappings_lock;
	size_t mapped_pages_count, mapped_entries_count;
};
#if 0
struct __inode {
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
	unsigned int sb_idx, fs_idx;
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

	/* shared mmappings */
	struct hash_table *physicals;
	mutex_t mappings_lock;
	int mapped_pages_count, mapped_entries_count;
};
#endif
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

int sys_getdepth(int fd);
int sys_getcwdlen();
int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3);
int sync_inode_tofs(struct inode *i);

int sys_unlink(const char *);
int sys_rmdir(const char *);
int vfs_inode_get_ref_count();
int sys_umount();

int fs_unlink(struct inode *node, const char *name, size_t namelen);
void vfs_icache_init();
int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen);
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen);
int fs_inode_pull(struct inode *node);
int fs_inode_push(struct inode *node);
struct inode *fs_path_resolve_inode(const char *path, int flags, int *error);
struct inode *fs_dirent_readinode(struct dirent *dir);
struct dirent *fs_resolve_path(const char *path, int flags);
void vfs_inode_get(struct inode *node);
struct inode *vfs_icache_get(struct filesystem *, uint32_t num);
int fs_inode_write(struct inode *node, size_t off, size_t count, const char *buf);
int fs_inode_read(struct inode *node, size_t off, size_t count, char *buf);
int sys_chroot(char *path);
void vfs_icache_put(struct inode *node);
void vfs_inode_set_dirty(struct inode *node);
#define FS_INODE_POPULATE 1
addr_t fs_inode_map_shared_physical_page(struct inode *node, addr_t virt, 
		size_t offset, int flags, int attrib);
addr_t fs_inode_map_private_physical_page(struct inode *node, addr_t virt,
		size_t offset, int attrib, size_t);
void fs_inode_map_region(struct inode *node, size_t offset, size_t length);
void fs_inode_sync_physical_page(struct inode *node, addr_t virt, size_t offset, size_t);
void fs_inode_unmap_region(struct inode *node, addr_t virt, size_t offset, size_t length);
void fs_inode_destroy_physicals(struct inode *node);

#endif
