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
#define DIRENT_UNLINK 1
struct dirent {
	int count;
	int flags;
	rwlock_t lock;
	struct inode *parent;
	struct filesystem *filesystem;
	uint32_t ino;
	char name[DNAME_LEN];
	size_t namelen;
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

	struct filesystem *filesystem, *mount;

	pipe_t *pipe;

	struct flock *flocks;
	mutex_t *flm;

	struct hash_table *physicals;
	mutex_t mappings_lock;
	size_t mapped_pages_count, mapped_entries_count;
};

int sys_getdepth(int fd);
int sys_getcwdlen();
int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3);

int sys_unlink(const char *);
int sys_rmdir(const char *);
int sys_umount();
int sys_chroot(char *path);
int sys_mkdir(const char *path, mode_t mode);
int sys_getdents(int, struct dirent_posix *, unsigned int);

int fs_unlink(struct inode *node, const char *name, size_t namelen);
int fs_link(struct inode *dir, struct inode *target, const char *name, size_t namelen);
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen);

void vfs_icache_init();
void vfs_inode_umount(struct inode *node);
int fs_icache_sync();
void vfs_inode_get(struct inode *node);
struct inode *vfs_inode_create();
struct inode *vfs_icache_get(struct filesystem *, uint32_t num);
void vfs_icache_put(struct inode *node);
void vfs_inode_set_dirty(struct inode *node);
void vfs_inode_set_needread(struct inode *node);
int vfs_inode_check_permissions(struct inode *node, int perm, int real);
void vfs_inode_del_dirent(struct inode *node, struct dirent *dir);
void vfs_inode_add_dirent(struct inode *node, struct dirent *dir);
struct dirent *vfs_inode_get_dirent(struct inode *node, const char *name, int namelen);
struct dirent *vfs_dirent_create(struct inode *node);
int vfs_dirent_release(struct dirent *dir);
void vfs_dirent_destroy(struct dirent *dir);
int vfs_dirent_acquire(struct dirent *dir);

int fs_inode_pull(struct inode *node);
int fs_inode_push(struct inode *node);
struct inode *fs_resolve_path_inode(const char *path, int flags, int *error);
struct inode *fs_dirent_readinode(struct dirent *dir, int);
struct dirent *fs_resolve_path(const char *path, int flags);
struct inode *fs_resolve_path_create(const char *path, int flags, mode_t mode, int *did_create);
struct dirent *fs_readdir(struct inode *node, size_t num);
struct inode *fs_read_root_inode(struct filesystem *fs);
int vfs_inode_chdir(struct inode *node);
int vfs_inode_chroot(struct inode *node);
void vfs_inode_mount(struct inode *node, struct filesystem *fs);


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

#endif
