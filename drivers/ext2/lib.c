#include <sea/sys/fcntl.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <modules/ext2.h>
int ext2_read_block(struct ext2_info *fs, uint64_t block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb);// + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(READ, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_block(struct ext2_info *fs, uint64_t block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb);// + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(WRITE, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_read_off(struct ext2_info *fs, off_t off, unsigned char *buf, size_t len)
{
	//off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(READ, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_off(struct ext2_info *fs, off_t off, unsigned char *buf, size_t len)
{
	//off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = dm_block_device_rw(WRITE, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}

