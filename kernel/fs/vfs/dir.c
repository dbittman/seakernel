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
	if(!path)
		return -EINVAL;
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
	while(i && i->parent && ((int)(strlen(path) + 
		strlen(i->name)) < max || max == -1))
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

int get_path_string(struct inode *p, char *buf, int len)
{
	if(!p || !buf || !len)
		return -EINVAL;
	return do_get_path_string(p, buf, len);
}

int get_pwd(char *buf, int sz)
{
	if(!buf) 
		return -EINVAL;
	return do_get_path_string(current_task->pwd, buf, sz == 0 ? -1 : sz);
}

int chroot(char *n)
{
	if(!n) 
		return -EINVAL;
	struct inode *i, *old = current_task->root;
	if(current_task->uid != GOD)
		return -EPERM;
	i = get_idir(n, 0);
	if(!i)
		return -ENOENT;
	if(!is_directory(i)) {
		iput(i);
		return -ENOTDIR;
	}
	current_task->root = i;
	iput(old);
	/* we call chdir here and not ichdir so that the count will again
	 * be incremented */
	chdir("/");
	return 0;
}

int ichdir(struct inode *i)
{
	if(!i)
		return -EINVAL;
	struct inode *old=current_task->pwd;
	if(!is_directory(i)) {
		iput(i);
		return -ENOTDIR;
	}
	if(!permissions(i, MAY_EXEC)) {
		iput(i);
		return -EACCES;
	}
	current_task->pwd = i;
	iput(old);
	return 0;
}

int chdir(char *path)
{
	if(!path) return -EINVAL;
	struct inode *i = get_idir(path, 0);
	if(!i) return -ENOENT;
	return ichdir(i);
}

struct inode *do_readdir(struct inode *i, int num)
{
	assert(i);
	int n = num;
	if(!is_directory(i))
		return 0;
	if(!permissions(i, MAY_READ))
		return 0;
	struct inode *c=0;
	if(!i->dynamic) {
		rwlock_acquire(&i->rwl, RWL_READER);
		struct llistnode *cur;
		ll_for_each_entry((&i->children), cur, struct inode *, c)
		{
			if(!n--) break;
		}
		if(num && cur == i->children.head)
			c=0;
		rwlock_release(&i->rwl, RWL_READER);
	}
	else if(i->i_ops && i->i_ops->readdir) {
		c = vfs_callback_readdir(i, num);
		if(c)
			c->count=1;
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
	if(!permissions(i, MAY_READ)) {
		iput(i);
		return 0;
	}
	struct inode *ret = do_readdir(i, num);
	iput(i);
	return ret;
}

struct inode *read_idir(struct inode *i, int num)
{
	if(!i)
		return 0;
	if(!permissions(i, MAY_READ))
		return 0;
	return do_readdir(i, num);
}
