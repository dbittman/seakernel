#include <kernel.h>
#include <fs.h>
#include <dev.h>
#include <asm/system.h>
#include <ll.h>
struct sblktbl *sb_table=0;
struct llist *mountlist;
mutex_t ml_mutex, sb_mutex;

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
	mutex_off(&mountlist->lock);
	if(n) return 0;
	if(m == mountlist->head && _n) return 0;
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
	create_mutex(&sb_mutex);
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
	mutex_on(&sb_mutex);
	struct sblktbl *o = sb_table;
	sb_table = sb;
	sb->next=o;
	if(o) o->prev = sb;
	sb->prev=0;
	mutex_off(&sb_mutex);
	return 0;
}

struct inode *sb_callback(char *fsn, dev_t dev, u64 block, char *n)
{
	struct sblktbl *s = sb_table;
	mutex_on(&sb_mutex);
	while(s) {
		if(!strcmp(fsn, s->name))
			break;
		s=s->next;
	}
	mutex_off(&sb_mutex);
	return s ? s->sb_load(dev, block, n) : 0;
}

struct inode *sb_check_all(dev_t dev, u64 block, char *n)
{
	struct inode *i=0;
	struct sblktbl *s = sb_table;
	mutex_on(&sb_mutex);
	while(s) {
		mutex_off(&sb_mutex);
		i=s->sb_load(dev, block, n);
		if(i)
			return i;
		mutex_on(&sb_mutex);
		s=s->next;
	}
	mutex_off(&sb_mutex);
	return 0;
}

int unregister_sbt(char *name)
{
	struct sblktbl *s = sb_table;
	mutex_on(&sb_mutex);
	while(s) {
		if(!strcmp(name, s->name))
			break;
		s=s->next;
	}
	if(s)
	{
		if(s->prev)
			s->prev->next = s->next;
		else
			sb_table = s->next;
		if(s->next)
			s->next->prev = s->prev;
		kfree(s);
	}
	mutex_off(&sb_mutex);
	return 0;
}
