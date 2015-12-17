#include <sea/types.h>
#include <sea/fs/fs.h>
#include <sea/fs/inode.h>
#include <sea/fs/kerfs.h>
#include <sea/kernel.h>
#include <stdatomic.h>
#include <sea/mm/kmalloc.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
static _Atomic uint32_t fsids = 0;

static struct hash fsdrivershash;
static struct linkedlist fsdriverslist;

static struct linkedlist fslist;
struct filesystem *devfs;

void fs_fsm_init(void)
{
	linkedlist_create(&fsdriverslist, LINKEDLIST_MUTEX);
	linkedlist_create(&fslist, LINKEDLIST_MUTEX);
	hash_create(&fsdrivershash, 0, 10);
	devfs = fs_filesystem_create();
	linkedlist_insert(&fslist, &devfs->listnode, devfs);
}

struct __kfs_data {
	size_t *offset, length, *current;
	unsigned char *buffer;
};

static void __kfs_mnt_rp_write(struct linkedentry *entry, void *__data)
{
	struct filesystem *fs = entry->obj;
	struct __kfs_data *data = __data;
	char *point = fs->pointname;
	if(current_process->root->filesystem == fs)
		point = "/";
	KERFS_PRINTF((*data->offset), data->length, data->buffer, (*data->current),
			"%s %s %s active,0x%x 0 0\n",
			fs->nodename, point, fs->type, fs->opts);
}

int kerfs_mount_report(int direction, void *param, size_t size, size_t offset, size_t length, unsigned char *buf)
{
	size_t current = 0;
	struct __kfs_data data = { .offset = &offset,
							   .length = length, 
							   .current = &current, 
							   .buffer = buf };
	KERFS_PRINTF(offset, length, buf, current,
			"DEVICE MTPOINT TYPE OPTIONS\n");
	linkedlist_apply_data(&fslist, __kfs_mnt_rp_write, &data);
	return current;
}

struct inode *fs_read_root_inode(struct filesystem *fs)
{
	struct inode *node = vfs_icache_get(fs, fs->root_inode_id);
	if(!node) {
		printk(KERN_ERROR, "failed to read root inode of filesystem %d\n", fs->id);
	}
	return node;
}

bool __fs_init_mount_find_driver(struct linkedentry *entry, void *data)
{
	struct filesystem *fs = data;
	struct fsdriver *driver = entry->obj;
	if(!driver->mount(fs)) {
		fs->driver = driver;
		strncpy(fs->type, driver->name, 128);
		return true;
	}
	return false;
}

int fs_filesystem_init_mount(struct filesystem *fs, char *point, char *node, char *type, int opts)
{
	if(type)
		strncpy(fs->type, type, 128);
	fs->opts = opts;
	fs->pointname = kmalloc(strlen(point) + 1);
	strncpy(fs->pointname, point, strlen(point));
	if(!strcmp(fs->type, "devfs") || !strcmp(fs->type, "tmpfs")) {
		fs->nodename = kmalloc(strlen(fs->type) + 1);
		strncpy(fs->nodename, fs->type, strlen(fs->type));
		if(fs->type[0] == 'd')
			return 0;
	} else {
		fs->nodename = kmalloc(strlen(node) + 1);
		strncpy(fs->nodename, node, strlen(node));
	}
	int err;
	struct inode *i = fs_path_resolve_inode(node, 0, &err);
	if(!i)
		return err;
	fs->dev = i->phys_dev;
	fs->node = i;
	if(!strcmp(fs->type, "tmpfs"))
		return ramfs_mount(fs);
	struct fsdriver *fd = 0;

	if(type) {
		if((fd = hash_lookup(&fsdrivershash, type, strlen(type))) == NULL)
			return -EINVAL;
		fs->driver = fd;
		return fd->mount(fs);
	} else {
		if(linkedlist_find(&fsdriverslist, __fs_init_mount_find_driver, fs))
			return 0;
	}
	return -EINVAL;
}

int fs_umount(struct filesystem *fs)
{
	assert(fs);
	if(fs->driver && fs->driver->umount)
		fs->driver->umount(fs);
	vfs_inode_umount(fs->point);
	if(fs != devfs)
		linkedlist_remove(&fslist, &fs->listnode);
	return 0;
}

static void __fs_unmount_all_applic(struct linkedentry *entry)
{
	if(entry->obj != devfs)
		fs_umount(entry->obj);
}

void fs_unmount_all(void)
{
	linkedlist_apply(&fslist, __fs_unmount_all_applic);
}

int fs_mount(struct inode *pt, struct filesystem *fs)
{
	if(!strcmp(fs->type, "devfs")) {
		fs = devfs;
	} else {
		linkedlist_insert(&fslist, &fs->listnode, fs);
	}
	vfs_inode_mount(pt, fs);
	return 0;
}

struct filesystem *fs_filesystem_create (void)
{
	struct filesystem *fs = kmalloc(sizeof(struct filesystem));
	fs->id = atomic_fetch_add_explicit(&fsids, 1, memory_order_relaxed);
	return fs;
}

void fs_filesystem_destroy(struct filesystem *fs)
{
	if(fs->nodename)
		kfree(fs->nodename);
	if(fs->pointname)
		kfree(fs->pointname);
	kfree(fs);
}

int fs_filesystem_register(struct fsdriver *fd)
{
	linkedlist_insert(&fsdriverslist, &fd->listnode, fd);
	return hash_insert(&fsdrivershash, fd->name, strlen(fd->name), &fd->hash_elem, fd);
}

int fs_filesystem_unregister(struct fsdriver *fd)
{
	linkedlist_remove(&fsdriverslist, &fd->listnode);
	return hash_delete(&fsdrivershash, fd->name, strlen(fd->name));
}

