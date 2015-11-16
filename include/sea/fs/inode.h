#ifndef __SEA_FS_INODE_H
#define __SEA_FS_INODE_H

#include <sea/types.h>
#include <sea/mutex.h>
#include <sea/rwlock.h>
#include <sea/lib/linkedlist.h>
#include <sea/fs/flock.h>
#include <sea/fs/stat.h>
#include <sea/lib/hash.h>
#include <sea/lib/queue.h>
#include <sea/fs/fs.h>
#include <stdbool.h>
#define MAY_EXEC      0100
#define MAY_WRITE     0200
#define MAY_READ      0400

typedef struct {
	struct inode *root;
	struct inode *parent;
} mount_pt_t;

struct pipe;

#define INODE_NEEDREAD 1
#define INODE_DIRTY    2
#define INODE_INUSE    4
#define INODE_NOLRU    8 /* On calling vfs_icache_put, don't move to LRU, immediately destroy */
#define INODE_PCACHE   0x10

#define RESOLVE_NOLINK  1
#define RESOLVE_NOMOUNT 2

struct pty;

struct file;
struct kdevice {
	void *data;
	int type;
	ssize_t (*rw)(int dir, struct file *file, off_t off, uint8_t *buffer, size_t length);
	int (*select)(struct file *file, int rw);
	void (*open)(struct file *file);
	void (*close)(struct file *file);
	void (*destroy)(struct inode *inode);
	int (*ioctl)(struct file *file, int cmd, long arg);
};

struct inode {
	struct rwlock lock, metalock;
	struct queue_item lru_item;
	struct linkedentry dirty_item;
	struct linkedentry inuse_item;
	struct hash dirents;
	struct filesystem *filesystem;
	
	_Atomic int count;
	_Atomic int flags;
	
	dev_t phys_dev;
	struct filesystem *mount;

	struct pipe *pipe;
	struct pty *pty; /* TODO: consoledate these into a union */
	struct socket *socket;
	struct kdevice kdev;
	struct blocklist readblock, writeblock;

	/* filesystem data */
	mode_t mode;
	uid_t uid;
	gid_t gid;
	short nlink;
	size_t length;
	time_t ctime, atime, mtime;
	size_t blocksize;
	size_t nblocks;
	uint32_t id;
	uint32_t key[2];
	struct hashelem hash_elem;

	/* mmap stuff */
	struct hash physicals;
	struct mutex mappings_lock;
	size_t mapped_pages_count, mapped_entries_count;
};

int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3);

int sys_unlink(const char *);
int sys_rmdir(const char *);
int sys_umount();
int sys_chroot(char *path);

int fs_unlink(struct inode *node, const char *name, size_t namelen);
int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen, bool allow_incomplete_directories);

void vfs_icache_init();
void vfs_inode_umount(struct inode *node);
int fs_icache_sync();
void vfs_inode_get(struct inode *node);
struct inode *vfs_inode_create();
struct inode *vfs_icache_get(struct filesystem *, uint32_t num);
void vfs_icache_put(struct inode *node);
void vfs_inode_set_dirty(struct inode *node);
void vfs_inode_unset_dirty(struct inode *node);
void vfs_inode_set_needread(struct inode *node);
int vfs_inode_check_permissions(struct inode *node, int perm, int real);
void vfs_inode_del_dirent(struct inode *node, struct dirent *dir);
void vfs_inode_add_dirent(struct inode *node, struct dirent *dir);

int fs_inode_pull(struct inode *node);
int fs_inode_push(struct inode *node);

struct inode *fs_path_resolve_create_get(const char *path,
		int flags, mode_t mode, int *result, struct dirent **dirent);
struct inode *fs_path_resolve_create(const char *path,
		int flags, mode_t mode, int *result);
struct dirent *fs_path_resolve(const char *path, int flags, int *result);
struct dirent *fs_do_path_resolve(struct inode *start, const char *path, int, int *result);
struct inode *fs_path_resolve_inode(const char *path, int flags, int *error);

struct dirent *fs_resolve_symlink(struct dirent *dir, struct inode *node, int level, int *err);
int fs_resolve_iter_symlink(struct dirent **dir, struct inode **node, int);
size_t fs_inode_reclaim_lru();
int fs_inode_dirempty(struct inode *dir);

struct inode *fs_read_root_inode(struct filesystem *fs);
int vfs_inode_chdir(struct inode *node);
int vfs_inode_chroot(struct inode *node);
void vfs_inode_mount(struct inode *node, struct filesystem *fs);
struct inode *fs_resolve_mount(struct inode *node);

ssize_t fs_inode_write(struct inode *node, size_t off, size_t count, const char *buf);
ssize_t fs_inode_read(struct inode *node, size_t off, size_t count, char *buf);

#define FS_INODE_POPULATE 1
addr_t fs_inode_map_shared_physical_page(struct inode *node, addr_t virt, 
		size_t offset, int flags, int attrib);
addr_t fs_inode_map_private_physical_page(struct inode *node, addr_t virt,
		size_t offset, int attrib, size_t);
void fs_inode_map_region(struct inode *node, size_t offset, size_t length);
void fs_inode_sync_physical_page(struct inode *node, addr_t virt, size_t offset, size_t);
void fs_inode_unmap_region(struct inode *node, addr_t virt, size_t offset, size_t length);
void fs_inode_destroy_physicals(struct inode *node);
void fs_inode_sync_region(struct inode *node, addr_t virt, size_t offset, size_t length);

#endif
