#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/asm/system.h>
#include <sea/ll.h>
#include <sea/fs/mount.h>
#include <sea/fs/callback.h>
#include <sea/cpu/atomic.h>
struct llist *mountlist;
static struct llist *sblist;

static unsigned int fs_idx_counter=0;

struct inode *fs_get_filesystem(int _n)
{
	int n=_n;
	struct mountlst *m=0;
	struct llistnode *cur;
	rwlock_acquire(&mountlist->rwl, RWL_READER);
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		if(!n) break;
		n--;
	}
	if(!cur) {
		rwlock_release(&mountlist->rwl, RWL_READER);
		return 0;
	}
	rwlock_release(&mountlist->rwl, RWL_READER);
	if(n) return 0;
	return m ? m->i : 0;
}

struct mountlst *fs_get_mount(struct inode *i)
{
	struct mountlst *m=0;
	struct llistnode *cur;
	rwlock_acquire(&mountlist->rwl, RWL_READER);
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		if(m->i == i) break;
	}
	rwlock_release(&mountlist->rwl, RWL_READER);
	return (m && (m->i == i)) ? m : 0;
}

int fs_init_superblock_table()
{
	mountlist = ll_create(0);
	sblist = ll_create(0);
	return 0;
}

void fs_unmount_all()
{
	struct mountlst *m;
	struct llistnode *cur, *next;
	ll_for_each_entry_safe(mountlist, cur, next, struct mountlst *, m)
	{
		vfs_do_unmount(m->i->mount_parent, 1);
		ll_remove(mountlist, cur);
	}
}

void fs_do_sync_of_mounted()
{
	rwlock_acquire(&mountlist->rwl, RWL_READER);
	struct mountlst *m;
	struct llistnode *cur;
	ll_for_each_entry(mountlist, cur, struct mountlst *, m)
	{
		vfs_callback_fssync(m->i);
	}
	rwlock_release(&mountlist->rwl, RWL_READER);
}

unsigned int fs_register_filesystem(char *name, int ver, int (*sbl)(dev_t,u64,char *))
{
	struct sblktbl *sb = (struct sblktbl *)kmalloc(sizeof(struct sblktbl));
	sb->version = (char)ver;
	sb->sb_load = (struct inode * (*)(dev_t,u64,char*))sbl;
	strncpy(sb->name, name, 16);
	sb->node = ll_insert(sblist, sb);
	return add_atomic(&fs_idx_counter, 1);
}

struct inode *fs_filesystem_callback(char *fsn, dev_t dev, u64 block, char *n)
{
	struct llistnode *cur;
	struct sblktbl *s;
	rwlock_acquire(&sblist->rwl, RWL_READER);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		if(!strcmp(fsn, s->name))
		{
			struct inode *i = s->sb_load(dev, block, n);
			rwlock_release(&sblist->rwl, RWL_READER);
			return i;
		}
	}
	rwlock_release(&sblist->rwl, RWL_READER);
	return 0;
}

struct inode *fs_filesystem_check_all(dev_t dev, u64 block, char *n)
{
	struct llistnode *cur;
	struct sblktbl *s;
	rwlock_acquire(&sblist->rwl, RWL_READER);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		struct inode *i = s->sb_load(dev, block, n);
		if(i) {
			rwlock_release(&sblist->rwl, RWL_READER);
			return i;
		}
	}
	rwlock_release(&sblist->rwl, RWL_READER);
	return 0;
}

int fs_unregister_filesystem(char *name)
{
	struct llistnode *cur;
	struct sblktbl *s;
	rwlock_acquire(&sblist->rwl, RWL_WRITER);
	ll_for_each_entry(sblist, cur, struct sblktbl *, s)
	{
		if(!strcmp(name, s->name))
		{
			ll_do_remove(sblist, s->node, 1);
			kfree(s);
			rwlock_release(&sblist->rwl, RWL_WRITER);
			return 0;
		}
	}
	rwlock_release(&sblist->rwl, RWL_WRITER);
	return -ENOENT;
}
