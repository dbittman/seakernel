#include <sea/sys/fcntl.h>
#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <modules/ext2.h>
int ext2_read_block(ext2_fs_t *fs, u64 block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb) + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(READ, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_block(ext2_fs_t *fs, u64 block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb) + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(WRITE, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_read_off(ext2_fs_t *fs, off_t off, unsigned char *buf, size_t len)
{
	off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(READ, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_off(ext2_fs_t *fs, off_t off, unsigned char *buf, size_t len)
{
	off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(WRITE, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}
