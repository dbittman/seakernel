#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/asm/system.h>
#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/ll.h>
#include <sea/cpu/atomic.h>
#include <sea/rwlock.h>
#include <sea/fs/mount.h>
#include <sea/fs/inode.h>
#include <sea/fs/callback.h>
#include <sea/fs/devfs.h>
#include <sea/fs/proc.h>
#include <sea/fs/ramfs.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>

static int do_mount(struct inode *i, struct inode *p)
{
	if(current_task->thread->effective_uid)
		return -EACCES;
	if(i == current_task->thread->root)
		return -EINVAL;
	if(!vfs_inode_is_directory(i))
		return -ENOTDIR;
	if(!vfs_inode_is_directory(p))
		return -EIO;
	rwlock_acquire(&i->rwl, RWL_WRITER);
	if(i->mount) {
		rwlock_release(&i->rwl, RWL_WRITER);
		return -EBUSY;
	}
	i->mount = (mount_pt_t *)kmalloc(sizeof(mount_pt_t));
	i->mount->root = p;
	i->mount->parent = i;
	add_atomic(&i->count, 1);
	p->mount_parent = i;
	rwlock_release(&i->rwl, RWL_WRITER);
	struct mountlst *ent = (void *)kmalloc(sizeof(struct mountlst));
	ent->node = ll_insert(mountlist, ent);
	ent->i = p;
	return 0;
}

static int mount(char *d, struct inode *p)
{
	if(!p)
		return -EINVAL;
	struct inode *i = fs_resolve_path_inode(d, 0);
	if(!i)
		return -ENOENT;
	return do_mount(i, p);
}

static int s_mount(char *name, int dev, u64 block, char *fsname, char *no)
{
	struct inode *i=0;
	if(!fsname || !*fsname)
		i=fs_filesystem_check_all(dev, block, no);
	else
		i=fs_filesystem_callback(fsname, dev, block, no);
	if(!i)
		return -EIO;
	i->count=1;
	i->dev = dev;
	int ret = mount(name, i);
	if(!ret) 
		return 0;
	vfs_callback_unmount(i, i->sb_idx);
	vfs_icache_put(i);
	return ret;
}

int sys_mount2(char *node, char *to, char *name, char *opts, int flags)
{
	if(!to) return -EINVAL;
	if(name && (!node || *node == '*')) {
		if(!strcmp(name, "devfs")) {
			iremove_nofree(devfs_root);
			return mount(to, devfs_root);
		}
		if(!strcmp(name, "procfs")) {
			iremove_nofree(procfs_root);
			return mount(to, procfs_root);
		}
		if(!strcmp(name, "tmpfs"))
			return mount(to, fs_init_tmpfs());
		return -EINVAL;
	}
	if(!node)
		return -EINVAL;
	struct inode *i=0;
	i = fs_resolve_path_inode(node, 0);
	if(!i)
		return -ENOENT;
	int dev = i->dev;
	vfs_icache_put(i);
	return s_mount(to, dev, 0, name, node);
}

int sys_mount(char *node, char *to)
{
	return sys_mount2(node, to, 0, 0, 0);
}

int vfs_do_unmount(struct inode *i, int flags)
{
	if(!i || !i->mount)
		return -EINVAL;
	if(current_task->thread->effective_uid)
		return -EACCES;
	if(!vfs_inode_is_directory(i))
		return -ENOTDIR;
	struct inode *m = i->mount->root;
	rwlock_acquire(&m->rwl, RWL_WRITER);
	if(m->count>1 && (!(flags&1) && !current_task->thread->effective_uid)) {
		rwlock_release(&m->rwl, RWL_WRITER);
		return -EBUSY;
	}
	rwlock_acquire(&i->rwl, RWL_WRITER);
	mount_pt_t *mt = i->mount;
	i->mount=0;
	vfs_callback_unmount(m, m->sb_idx);
	rwlock_release(&i->rwl, RWL_WRITER);
	m->mount_parent=0;
	struct mountlst *lst = fs_get_mount(m);
	ll_remove(mountlist, lst->node);
	kfree(lst);
	if(m != devfs_root && m != procfs_root)
		iremove_recur(m);
	else
		rwlock_release(&m->rwl, RWL_WRITER);
	kfree(mt);
	return 0;
}

int vfs_unmount(char *n, int flags)
{
	if(!n) return -EINVAL;
	struct inode *i=0;
	i = fs_resolve_path_inode(n, 0);
	if(!i)
		return -ENOENT;
	if(!i->mount_parent)
		return -ENOENT;
	vfs_icache_put(i);
	i = i->mount_parent;
	int ret = vfs_do_unmount(i, flags);
	if(!ret)
		vfs_icache_put(i);
	return ret;
}
