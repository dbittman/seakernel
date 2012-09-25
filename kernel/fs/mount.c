#include <kernel.h>
#include <fs.h>
#include <dev.h>
#include <asm/system.h>
#include <ll.h>
struct llist *mountlist, *sblist;

struct inode *get_sb_table(int _n)
{
	int n=_n;
	struct mountlst *m=0;
	struct llistnode *cur;
	mutex_on(&mountlist->lock);
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		if(!n) break;
		n--;
	}
	if(cur == mountlist->head && _n) {
		mutex_off(&mountlist->lock);
		return 0;
	}
	mutex_off(&mountlist->lock);
	if(n) return 0;
	return m ? m->i : 0;
}

struct mountlst *get_mount(struct inode *i)
{
	struct mountlst *m=0;
	struct llistnode *cur;
	mutex_on(&mountlist->lock);
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		if(m->i == i) break;
	}
	mutex_off(&mountlist->lock);
	return (m && (m->i == i)) ? m : 0;
}

int load_superblocktable()
{
	mountlist = ll_create(0);
	sblist = ll_create(0);
	return 0;
}

void unmount_all()
{
	struct mountlst *m;
	struct llistnode *cur, *next;
	mutex_on(&mountlist->lock);
	ll_for_each_entry_safe(mountlist, cur, next, struct mountlst *, m)
	{
		do_unmount(m->i->mount_parent, 1);
		ll_maybe_reset_loop(mountlist, cur, next);
		ll_remove(mountlist, cur);
	}
	mutex_off(&mountlist->lock);
}

void do_sync_of_mounted()
{
	mutex_on(&mountlist->lock);
	struct mountlst *m;
	struct llistnode *cur;
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		vfs_callback_fssync(m->i);
	}
	mutex_off(&mountlist->lock);
}

int register_sbt(char *name, int ver, int (*sbl)(dev_t,u64,char *))
{
	struct sblktbl *sb = (struct sblktbl *)kmalloc(sizeof(struct sblktbl));
	sb->version = (char)ver;
	sb->sb_load = (struct inode * (*)(dev_t,u64,char*))sbl;
	strncpy(sb->name, name, 16);
	sb->node = ll_insert(sblist, sb);
	return 0;
}

struct inode *sb_callback(char *fsn, dev_t dev, u64 block, char *n)
{
	struct llistnode *cur;
	struct sblktbl *s;
	mutex_on(&sblist->lock);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		if(!strcmp(fsn, s->name))
		{
			mutex_off(&sblist->lock);
			return s->sb_load(dev, block, n);
		}
	}
	mutex_off(&sblist->lock);
	return 0;
}

struct inode *sb_check_all(dev_t dev, u64 block, char *n)
{
	struct llistnode *cur;
	struct sblktbl *s;
	mutex_on(&sblist->lock);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		struct inode *i = s->sb_load(dev, block, n);
		if(i) {
			mutex_off(&sblist->lock);
			return i;
		}
	}
	mutex_off(&sblist->lock);
	return 0;
}

int unregister_sbt(char *name)
{
	struct llistnode *cur;
	struct sblktbl *s;
	mutex_on(&sblist->lock);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		if(!strcmp(name, s->name))
		{
			ll_remove(sblist, s->node);
			kfree(s);
			mutex_off(&sblist->lock);
			return 0;
		}
	}
	mutex_off(&sblist->lock);
	return -ENOENT;
}
