#include "ext2.h"
#include "sys/fcntl.h"
#include <kernel.h>
#include <fs.h>
#include <dev.h>

int ext2_read_block(ext2_fs_t *fs, unsigned block, unsigned char *buf)
{
	int off = block * ext2_sb_blocksize(fs->sb) + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = block_device_rw(READ, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_block(ext2_fs_t *fs, unsigned block, unsigned char *buf)
{
	int off = block * ext2_sb_blocksize(fs->sb) + fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = block_device_rw(WRITE, fs->dev, off, (char *)buf, ext2_sb_blocksize(fs->sb));
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_read_off(ext2_fs_t *fs, unsigned off, unsigned char *buf, unsigned len)
{
	off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = block_device_rw(READ, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}

int ext2_write_off(ext2_fs_t *fs, unsigned off, unsigned char *buf, unsigned len)
{
	off += fs->block*512;
	//mutex_on(&fs->ac_lock);
	int ret = block_device_rw(WRITE, fs->dev, off, (char *)buf, len);
	//mutex_off(&fs->ac_lock);
	return ret;
}
