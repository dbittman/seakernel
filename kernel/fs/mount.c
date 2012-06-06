#include <kernel.h>
#include <fs.h>
#include <dev.h>
#include <asm/system.h>
struct sblktbl *sb_table=0;
struct mountlst *ml=0;
mutex_t ml_mutex, sb_mutex;

struct inode *get_sb_table(int n)
{
	struct mountlst *m = ml;
	mutex_on(&ml_mutex);
	while(n-- && m) m = m->next;
	mutex_off(&ml_mutex);
	return m ? m->i : 0;
}

int load_superblocktable()
{
	create_mutex(&ml_mutex);
	create_mutex(&sb_mutex);
	return 0;
}

void add_mountlst(struct inode *n)
{
	assert(n);
	mutex_on(&ml_mutex);
	struct mountlst *m = (struct mountlst *)kmalloc(sizeof(struct mountlst));
	m->i = n;
	struct mountlst *o = ml;
	ml = m;
	m->next=o;
	if(o)
		o->prev = m;
	m->prev=0;
	mutex_off(&ml_mutex);
}

void remove_mountlst(struct inode *n)
{
	mutex_on(&ml_mutex);
	struct mountlst *m = ml;
	while(m && m->i != n)
		m=m->next;
	if(m)
	{
		if(m->prev)
			m->prev->next = m->next;
		else
			ml = m->next;
		if(m->next)
			m->next->prev=m->prev;
		kfree(m);
	}
	mutex_off(&ml_mutex);
}

void unmount_all()
{
	struct mountlst *m = ml;
	mutex_on(&ml_mutex);
	while(m) {
		do_unmount(m->i->r_mount_ptr, 1);
		struct mountlst *t = m->next;
		remove_mountlst(m->i);
		m=t;
	}
	mutex_off(&ml_mutex);
}

void do_sync_of_mounted()
{
	mutex_on(&ml_mutex);
	struct mountlst *m = ml;
	while(m) {
		vfs_callback_fssync(m->i);
		m = m->next;
	}
	mutex_off(&ml_mutex);
}

int register_sbt(char *name, int ver, int (*sbl)(int,int,char *))
{
	struct sblktbl *sb = (struct sblktbl *)kmalloc(sizeof(struct sblktbl));
	sb->version = (char)ver;
	sb->sb_load = (struct inode * (*)(int,int, char*))sbl;
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

struct inode *sb_callback(char *fsn, int dev, int block, char *n)
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

struct inode *sb_check_all(int dev, int block, char *n)
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
