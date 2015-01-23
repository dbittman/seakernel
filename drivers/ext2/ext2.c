#include <modules/ext2.h>
#include <sea/sys/fcntl.h>
#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/ll.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/types.h>
#include <sea/fs/mount.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/errno.h>

int ext2_mount(struct filesystem *seafs)
{
	struct ext2_info *fs = kmalloc(sizeof(struct ext2_info));
	fs->sb = kmalloc(1024);
	fs->dev = seafs->dev;
	mutex_create(&fs->bg_lock, 0);
	mutex_create(&fs->fs_lock, 0);
	fs->m_node = mutex_create(0, 0);
	fs->m_block = mutex_create(0, 0);

	//struct inode *in = vfs_get_idir(node, 0);
	//if(in && dev == -1)
//		dev = in->dev;
//	if(in && (int)in->dev != dev)
//		printk(4, "[ext2]: Odd...node device is different from given device...\n");
//	vfs_iput(in);
//	fs->block = block;
//	fs->dev = dev;
	fs->sb->block_size=0;
	//if(node)
	//	strncpy(fs->node, node, 16);
	ext2_read_block(fs, 1, (unsigned char *)fs->sb);
	if(fs->sb->magic != EXT2_SB_MAGIC) {
		return 0;
	}
	if(fs->sb->state == 2)
	{
		printk(5, "[ext2]: Filesystem has errors: ");
		if(fs->sb->errors == 2)
		{
			printk(5, "Mounting as read-only\n");
			fs->flags |= EXT2_FS_READONLY;
		} else if(fs->sb->errors == 3)
			panic(0, "ext2 mount failed!");
		else
			printk(5, "Ignoring...\n");
	}
	unsigned reqf = fs->sb->features_req;
	if(!(reqf&0x2) || (reqf & 0x1) || (reqf &0x4) || (reqf&0x8))
	{
		//printk(5, "[ext2]: Cannot mount %s due to feature flags\n", node);
		return 0;
	}
	unsigned rof = fs->sb->features_ro;
	if(ext2_sb_inodesize(fs->sb) != 128)
	{
		printk(5, "[ext2]: Inode size %d is not supported\n", ext2_sb_inodesize(fs->sb));
		return 0;
	}
	if(!(rof&0x1) || (rof & 0x2) || (rof&0x4))
	{
		//printk(5, "[ext2]: Filesystem on %s must be mounted read-only due to feature flags\n", node);
		fs->flags |= EXT2_FS_READONLY;
	}
	if(fs->sb->mount_count > fs->sb->max_mount_count)
		fs->sb->mount_count=0;
	fs->sb->mount_time = time_get_epoch();
	ext2_sb_update(fs, fs->sb);
	
	printk(0, "[ext2]: Optional features flags are %x\n", fs->sb->features_opt);
	if(fs->sb->features_opt & 0x4)
		printk(0, "[ext2]: Hmm...looks like an ext3 filesystem to me. Oh well. It should still work.\n");
	if(fs->sb->features_opt & 0x20)
		printk(0, "[ext2]: Hmm...directories have a hash index. I'll look into that later...\n");
	seafs->root_inode_id = 2;
	seafs->data = fs;
	seafs->fs_ops = &ext2_wrap_fsops;
	seafs->fs_inode_ops = &ext2_wrap_iops;
	return 0;
}

int ext2_unmount(struct filesystem *fs)
{
	return 0;
}

struct fsdriver ext2driver = {
	.flags = 0,
	.name = "ext2",
	.mount = ext2_mount,
	.umount = ext2_unmount,
};

int module_install()
{
	printk(1, "[ext2]: Registering filesystem\n");
	fs_filesystem_register(&ext2driver);
	return 0;
}

int module_exit()
{
	printk(1, "[ext2]: Unmounting all ext2 filesystems\n");
	fs_filesystem_unregister(&ext2driver);
	return 0;
}

int ext2_sb_update(struct ext2_info *fs, ext2_superblock_t *sb)
{
	int old = sb->block_size;
	fs->sb->block_size=0;
	sb->write_time = time_get_epoch();
	ext2_write_block(fs, 1, (unsigned char *)sb);
	fs->sb = sb;
	sb->block_size = old;
	return 0;
}
int module_deps(char *b)
{
	return CONFIG_VERSION_NUMBER;
}

int ext2_inode_type(mode_t mode)
{
#warning "fix this (doens't work)"
	/* We check if its a link first, cause the reg bit is set in link too */
	if(S_ISLNK(mode))
		return DET_SLINK;
	if(mode & 0x8000)
		return DET_REG;
	if(mode & 0x4000)
		return DET_DIR;
	if(mode & 0xC000)
		return DET_SOCK;
	if(mode & 0x2000)
		return DET_CHAR;
	if(mode & 0x6000)
		return DET_BLOCK;
	if(mode & 0x1000)
		return DET_FIFO;
	return DET_UNKNOWN;
}

