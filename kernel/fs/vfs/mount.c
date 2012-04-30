#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int do_mount(struct inode *i, struct inode *p)
{
	if(i == current_task->root)
		return -EINVAL;
	if(!permissions(i, MAY_WRITE))
		return -EACCES;
	if(!is_directory(i) || !is_directory(p))
		return -ENOTDIR;
	if(i->mount_ptr || i->r_mount_ptr)
		return -EBUSY;
	i->mount_ptr = p;
	p->r_mount_ptr = i;
	add_mountlst(p);
	return 0;
}

int mount(char *d, struct inode *p)
{
	if(!p) {
		kprintf("Couldn't mount (probably due to an unknown filesystem)\n");
		return -EINVAL;
	}
	struct inode *i = get_idir(d, 0);
	if(!i)
		return -ENOENT;
	int ret = do_mount(i, p);
	return ret;
}

int s_mount(char *name, int dev, int block, char *fsname, char *no)
{
	struct inode *i=0;
	if(!fsname || !*fsname)
	{
		//Look for one
		i=sb_check_all(dev, block, no);
	}
	else
	{
		i=sb_callback(fsname, dev, block, no);
	}
	if(!i)
		return -EIO;
	i->dev=dev; /* Mostly for swap...lol */
	int ret = mount(name, i);
	if(i && ret)
	{
		if(i->i_ops && i->i_ops->unmount)
			i->i_ops->unmount(i->sb_idx);
		iput(i);
	}
	return ret;
}

int sys_mount2(char *node, char *to, char *name, char *opts, int flags)
{
	if(!to) return -EINVAL;
	if(name && !node) {
		if(!strcmp(name, "devfs")) {
			iremove_nofree(devfs_root);
			return mount(to, devfs_root);
		}
		if(!strcmp(name, "procfs")) {
			iremove_nofree(procfs_root);
			return mount(to, procfs_root);
		}
		return -EINVAL;
	}
	if(!node)
		return -EINVAL;
	struct inode *i=0;
	i = get_idir(node, 0);
	if(!i) {
		return -ENOENT;
	}
	int dev = i->dev;
	iput(i);
	int ret = s_mount(to, dev, 0, name, node);
	return ret;
}

int sys_mount(char *node, char *to)
{
	return sys_mount2(node, to, 0, 0, 0);
}

int do_unmount(struct inode *i, int flags)
{
	if(!i)
		return -EINVAL;
	if(!permissions(i, MAY_READ))
		return -EACCES;
	if(!is_directory(i))
		return -ENOTDIR;
	struct inode *m = i->mount_ptr;
	mutex_on(&i->lock);
	i->mount_ptr=0;
	lock_scheduler();
	int c = recur_total_refs(m);
	if(c && (!(flags&1) && !current_task->uid))
	{
		i->mount_ptr=m;
		unlock_scheduler();
		mutex_off(&i->lock);
		return -EBUSY;
	}
	unlock_scheduler();
	if(i->mount_ptr->i_ops && i->mount_ptr->i_ops->unmount)
		i->mount_ptr->i_ops->unmount(i->mount_ptr->sb_idx);
	m->r_mount_ptr = 0;
	remove_mountlst(m);
	iremove_recur(m);
	iput(i);
	return 0;
}

int unmount(char *n, int flags)
{
	if(!n) return -EINVAL;
	struct inode *i=0;
	i = get_idir(n, 0);
	if(!i) {
		return -ENOENT;
	}
	struct inode *g;
	g = i->r_mount_ptr;
	if(!g) {
		iput(i);
		return -EINVAL;
	}
	change_icount(g, 1);
	change_icount(i, -1);
	int ret = do_unmount(g, flags);
	iput(i);
	return ret;
}
