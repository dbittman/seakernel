#include <sea/sys/fcntl.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/dm/block.h>
#include <modules/ext2.h>
int ext2_read_block(struct ext2_info *fs, uint64_t block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb);// + fs->block*512;
	struct file f;
	f.inode = fs->filesys->node;
	int ret = fs_file_pread(&f, off, buf, ext2_sb_blocksize(fs->sb));
	return ret;
}

int ext2_write_block(struct ext2_info *fs, uint64_t block, unsigned char *buf)
{
	off_t off = block * ext2_sb_blocksize(fs->sb);// + fs->block*512;
	struct file f;
	f.inode = fs->filesys->node;
	int ret = fs_file_pwrite(&f, off, buf, ext2_sb_blocksize(fs->sb));
	return ret;
}

int ext2_read_off(struct ext2_info *fs, off_t off, unsigned char *buf, size_t len)
{
	struct file f;
	f.inode = fs->filesys->node;
	int ret = fs_file_pread(&f, off, buf, len);
	return ret;
}

int ext2_write_off(struct ext2_info *fs, off_t off, unsigned char *buf, size_t len)
{
	struct file f;
	f.inode = fs->filesys->node;
	int ret = fs_file_pwrite(&f, off, buf, len);
	return ret;
}

