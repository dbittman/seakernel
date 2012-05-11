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
	if(i != current_task->root && i->r_mount_ptr)
		i = i->r_mount_ptr;
	char tmp[max * sizeof(char) +1];
	memset(tmp, 0, max * sizeof(char) +1);
	while(i && i->parent != i && i->parent && ((int)(strlen(path) + strlen(i->name)) < max || max == -1))
	{
		if(i->r_mount_ptr)
			i = i->r_mount_ptr;
		strcpy(tmp, path);
		sprintf(path, "%s/%s", i->name, tmp);
		i = i->parent;
		if(i == current_task->root)
			break;
		if(i->r_mount_ptr)
			i = i->r_mount_ptr;
		if(i == current_task->root)
			break;
	}
	strcpy(tmp, path);
	strcpy(path, "/");
	strcat(path, tmp);
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

int do_chroot(struct inode *i)
{
	mutex_on(&i->lock);
	current_task->root = i;
	mutex_off(&i->lock);
	return 0;
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
	mutex_on(&i->lock);
	i->required++;
	mutex_off(&i->lock);
	do_chroot(i);
	mutex_on(&old->lock);
	old->required++;
	mutex_off(&old->lock);
	iput(old);
	chdir("/");
	return 0;
}

int do_chdir(struct inode *i)
{
	if(!i) return -EINVAL;
	if(!is_directory(i)) return -ENOTDIR;
	if(!permissions(i, MAY_READ)) return -EACCES;
	current_task->pwd = i;
	return 0;
}

int chdir(char *n)
{
	if(!n)
		return -EINVAL;
	struct inode *i=0;
	struct inode *old = current_task->pwd;
	i = get_idir(n, 0);
	if(!i)
		return -ENOENT;
	int ret = do_chdir(i);
	if(!ret) {
		mutex_on(&old->lock);
		old->required--;
		mutex_off(&old->lock);
		iput(old);
	}
	mutex_on(&current_task->pwd->lock);
	current_task->pwd->required++;
	mutex_off(&current_task->pwd->lock);
	return ret;
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
		old = i->i_ops->readdir(i, num);
		if(!old) 
			return 0;
		if(old && old->r_mount_ptr) old = old->r_mount_ptr;
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
	if(!permissions(i, MAY_EXEC))
		return 0;
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
	if(i->child && get_uid() != 0) {
		iput(i);
		return -EACCES;
	}
	int ret=-EINVAL;
	if(i->i_ops)
		if(i->i_ops->rmdir)
			ret=(i->i_ops->rmdir(i));
	iput(i);
	return ret;
}
