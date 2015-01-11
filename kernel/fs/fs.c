#include <sea/types.h>
#include <sea/fs/fs.h>
#include <sea/kernel.h>

void fs_fssync(struct filesystem *fs)
{

}

struct inode *fs_read_root_inode(struct filesystem *fs)
{
	struct inode *node = vfs_icache_get(fs, fs->root_inode_id);
	assert(node);
	if(fs_inode_pull(node))
		return 0;
	return node;
}


