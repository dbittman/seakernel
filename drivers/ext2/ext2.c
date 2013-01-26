#include "ext2.h"
#include <sys/fcntl.h>
#include <kernel.h>
#include <fs.h>
#include <dev.h>
ext2_fs_t *fs_list;
unsigned fs_num=0;
mutex_t *fsll;
ext2_fs_t *get_new_fsvol()
{
	ext2_fs_t *fs = (ext2_fs_t *)kmalloc(sizeof(ext2_fs_t));
	mutex_acquire(fsll);
	ext2_fs_t *old = fs_list;
	fs->next = old;
	if(old) old->prev = fs;
	fs->prev=0;
	fs_list = fs;
	fs->flag=fs_num++;
	mutex_release(fsll);
	fs->sb = (ext2_superblock_t *)kmalloc(1024);
	fs->block_prev_alloc=0;
	fs->read_only=0;
	fs->m_node = mutex_create(0);
	fs->m_block = mutex_create(0);
	mutex_create(&fs->ac_lock);
	char tm[32];
	sprintf(tm, "ext2-%d", fs_num);
	fs->cache = get_empty_cache(0, tm);
	return fs;
}

ext2_fs_t *get_fs(int v)
{
	ext2_fs_t *f = fs_list;
	while(f)
	{
		if(f->flag == v)
			return f;
		f=f->next;
	}
	return 0;
}

void release_fsvol(ext2_fs_t *fs)
{
	if(!fs) return;
	mutex_acquire(fsll);
	if(fs->prev)
		fs->prev->next = fs->next;
	else
		fs_list = fs->next;
	if(fs->next)
		fs->next->prev=fs->prev;
	mutex_release(fsll);
	kfree(fs->sb);
	fs->m_node->pid=-1;
	fs->m_block->pid=-1;
	mutex_destroy(fs->m_node);
	mutex_destroy(fs->m_block);
	mutex_destroy(&fs->ac_lock);
	destroy_cache(fs->cache);
	kfree(fs);
}

struct inode *ext2_mount(dev_t dev, u64 block, char *node)
{
	ext2_fs_t *fs = get_new_fsvol();
	if(!fs) {
		printk(5, "[ext2]: Unable to allocate new filesystem!\n");
		release_fsvol(fs);
		return 0;
	}
	struct inode *in = get_idir(node, 0);
	if(in && dev == -1)
		dev = in->dev;
	if(in && (int)in->dev != dev)
		printk(4, "[ext2]: Odd...node device is different from given device...\n");
	rwlock_acquire(&in->rwl, RWL_WRITER);
	iput(in);
	fs->block = block;
	fs->dev = dev;
	fs->sb->block_size=0;
	if(node)
		strncpy(fs->node, node, 16);
	ext2_read_block(fs, 1, (unsigned char *)fs->sb);
	if(fs->sb->magic != EXT2_SB_MAGIC) {
		release_fsvol(fs);
		return 0;
	}
	if(fs->sb->state == 2)
	{
		printk(5, "[ext2]: Filesystem has errors: ");
		if(fs->sb->errors == 2)
		{
			printk(5, "Mounting as read-only\n");
			fs->read_only=1;
		} else if(fs->sb->errors == 3)
			panic(0, "ext2 mount failed!");
		else
			printk(5, "Ignoring...\n");
	}
	unsigned reqf = fs->sb->features_req;
	if(!(reqf&0x2) || (reqf & 0x1) || (reqf &0x4) || (reqf&0x8))
	{
		release_fsvol(fs);
		printk(5, "[ext2]: Cannot mount %s due to feature flags\n", node);
		return 0;
	}
	unsigned rof = fs->sb->features_ro;
	if(ext2_sb_inodesize(fs->sb) != 128)
	{
		release_fsvol(fs);
		printk(5, "[ext2]: Inode size %d is not supported\n", ext2_sb_inodesize(fs->sb));
		return 0;
	}
	if(!(rof&0x1) || (rof & 0x2) || (rof&0x4))
	{
		printk(5, "[ext2]: Filesystem on %s must be mounted read-only due to feature flags\n", node);
		fs->read_only=1;
	}
	ext2_inode_t root;
	ext2_inode_read(fs, 2, &root);
	fs->root = create_sea_inode(&root, "ext2");
	strncpy(fs->root->node_str, node, 128);
	if(fs->sb->mount_count > fs->sb->max_mount_count)
		fs->sb->mount_count=0;
	fs->sb->mount_time = get_epoch_time();
	ext2_sb_update(fs, fs->sb);
	
	printk(0, "[ext2]: Optional features flags are %x\n", fs->sb->features_opt);
	if(fs->sb->features_opt & 0x4)
		printk(0, "[ext2]: Hmm...looks like an ext3 filesystem to me. Oh well. It should still work.\n");
	if(fs->sb->features_opt & 0x20)
		printk(0, "[ext2]: Hmm...directories have a hash index. I'll look into that later...\n");
	return fs->root;
}

int ext2_unmount(struct inode *i, int v)
{
	ext2_fs_t *fs = get_fs(v);
	if(!fs)
		return -EINVAL;
	release_fsvol(fs);
	return 0;
}

int module_install()
{
	printk(1, "[ext2]: Registering filesystem\n");
	fs_list=0;
	fsll = mutex_create(0);
	register_sbt("ext2", 2, (int (*)(dev_t,u64,char*))ext2_mount);
	return 0;
}

int module_exit()
{
	printk(1, "[ext2]: Unmounting all ext2 filesystems\n");
	int i=0;
	while(fs_list && !ext2_unmount(0, fs_list->flag));
	unregister_sbt("ext2");
	mutex_destroy(fsll);
	return 0;
}

int ext2_sb_update(ext2_fs_t *fs, ext2_superblock_t *sb)
{
	int old = sb->block_size;
	fs->sb->block_size=0;
	sb->write_time = get_epoch_time();
	ext2_write_block(fs, 1, (unsigned char *)sb);
	fs->sb = sb;
	sb->block_size = old;
	return 0;
}
int module_deps(char *b)
{
	return KVERSION;
}

int ext2_inode_type(ext2_inode_t *in)
{
	/* We check if its a link first, cause the reg bit is set in link too */
	if(S_ISLNK(in->mode))
		return DET_SLINK;
	if(in->mode & 0x8000)
		return DET_REG;
	if(in->mode & 0x4000)
		return DET_DIR;
	if(in->mode & 0xC000)
		return DET_SOCK;
	if(in->mode & 0x2000)
		return DET_CHAR;
	if(in->mode & 0x6000)
		return DET_BLOCK;
	if(in->mode & 0x1000)
		return DET_FIFO;
	return DET_UNKNOWN;
}
