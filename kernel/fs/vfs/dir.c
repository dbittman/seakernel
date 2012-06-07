#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <asm/system.h>
#include <dev.h>
#include <fs.h>

int do_get_path_string(struct inode *p, char *path, int max)
{
	if(!p)
		return -EINVAL;
	if(max == -1)
		max = 2048;
	struct inode *i = p;
	if(!path) {
		return -EINVAL;
	}
	*path=0;
	if(!p)
		return 0;
	if(p->parent == kproclist)
	{
		sprintf(path, "%s", p->name);
		return 0;
	}
	if(i != current_task->root && i->mount_parent)
		i = i->mount_parent;
	char tmp[max * sizeof(char) +1];
	memset(tmp, 0, max * sizeof(char) +1);
	while(i && i->parent != i && i->parent && ((int)(strlen(path) + strlen(i->name)) < max || max == -1))
	{
		if(i->mount_parent)
			i = i->mount_parent;
		strncpy(tmp, path, max * sizeof(char) +1);
		sprintf(path, "%s/%s", i->name, tmp);
		i = i->parent;
		if(i == current_task->root)
			break;
		if(i->mount_parent)
			i = i->mount_parent;
		if(i == current_task->root)
			break;
	}
	strncpy(tmp, path, max * sizeof(char) +1);
	strncpy(path, "/", max - (strlen(tmp)+1));
	strncat(path, tmp, max);
	return 0;
}

int get_path_string(struct inode *p, char *buf)
{
	if(!p || !buf)
		return -EINVAL;
	return do_get_path_string(p, buf, -1);
}

int get_pwd(char *buf, int sz)
{
	if(!buf) 
		return -EINVAL;
	return do_get_path_string(current_task->pwd, buf, sz == 0 ? -1 : sz);
}

int chroot(char *n)
{
	if(!n) return -EINVAL;
	struct inode *i=0;
	struct inode *old = current_task->root;
	if(current_task->uid != GOD)
		return -EPERM;
	i = get_idir(n, 0);
	if(!i)
		return -ENOENT;
	if(!is_directory(i)) {
		iput(i);
		return -ENOTDIR;
	}
	if(!permissions(i, MAY_READ)) {
		iput(i);
		return -EACCES;
	}
	current_task->root = i;
	change_ireq(i, 1);
	change_ireq(old, -1);
	iput(old);
	chdir("/");
	return 0;
}

int chdir(char *n)
{
	if(!n)
		return -EINVAL;
	struct inode *i=0;
	struct inode *old = current_task->pwd;
	i = get_idir(n, 0);
	if(!i) return -ENOENT;
	if(!is_directory(i)) {
		iput(i);
		return -ENOTDIR;
	}
	if(!permissions(i, MAY_READ)) {
		iput(i);
		return -EACCES;
	}
	current_task->pwd = i;
	change_ireq(i, 1);
	change_ireq(old, -1);
	iput(old);
	return 0;
}

struct inode *do_readdir(struct inode *i, int num)
{
	assert(i);
	int n = num;
	if(!is_directory(i))
		return 0;
	if(!permissions(i, MAY_READ))
		return 0;
	struct inode *c = i->child;
	if(!i->dynamic) {
		mutex_on(&i->lock);
		while(c && n) c = c->next, --n;
		mutex_off(&i->lock);
	}
	else if(i->i_ops && i->i_ops->readdir) {
		struct inode *old=0;
		old = vfs_callback_readdir(i, num);
		if(!old) 
			return 0;
		old->count=1;
		c = old;
	}
	return c;
}

struct inode *read_dir(char *n, int num)
{
	if(!n) return 0;
	struct inode *i=0;
	i = get_idir(n, 0);
	if(!i)
		return 0;
	if(!permissions(i, MAY_EXEC)) {
		iput(i);
		return 0;
	}
	struct inode *ret = do_readdir(i, num);
	iput(i);
	return ret;
}

int rmdir(char *f)
{
	if(!f)
		return -EINVAL;
	struct inode *i;
	i = get_idir(f, 0);
	if(!i)
		return -ENOENT;
	if(!permissions(i, MAY_WRITE)) {
		iput(i);
		return -EACCES;
	}
	if(!is_directory(i)) {
		iput(i);
		return -ENOTDIR;
	}
	int ret = vfs_callback_rmdir(i);
	iput(i);
	return ret;
}
