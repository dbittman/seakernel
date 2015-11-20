#include <sea/dm/dev.h>
#include <sea/fs/inode.h>
#include <sea/dm/block.h>
#include <sea/loader/symbol.h>
#include <modules/psm.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

struct partition {
	char flag;
	char ext;
	char i_dont_care[2];
	char sysid;
	char again_dont_care[3];
	unsigned int start_lba;
	unsigned int length;
}__attribute__((packed));

int enumerate_partitions(int num, struct inode *node, struct partition *part)
{
	unsigned char buf[512];
	struct file f;
	f.inode = node;
	int r = fs_file_pread(&f, 0, buf, 512);
	struct partition ptable[4];
	memcpy(ptable, buf+0x1BE, 64);
	int i;
	for(i=0;i<4;i++)
	{
		if(ptable[i].sysid == 0xF || ptable[i].sysid == 0x5)
		{
			return 0;
		}
		if(!num--) {
			memcpy(part, &ptable[i], sizeof(*part));
			part->ext=0;
			return 1;
		}
	}
	return 0;
}

int part_get_partition(struct inode *node, uint64_t *start, size_t *len, int n)
{
	struct partition p;
	int ret = enumerate_partitions(n, node, &p);
	if(!p.sysid) return 0;
	if(ret) {
		*start = p.start_lba;
		*len = p.length;
	}
	return ret;
}
#include <sea/dm/blockdev.h>
int module_install(void)
{
	uint64_t start;
	size_t len;
	int i=0;
	int err;
	/* TODO: check for all block devices */
	struct inode *master = fs_path_resolve_inode("/dev/ada", 0, &err);
	while(part_get_partition(master, &start, &len, i)) {
		char name[128];
		snprintf(name, 128, "/dev/ada%d", i+1);
		sys_mknod(name, S_IFBLK | 0600, 0);
		struct inode *node = fs_path_resolve_inode(name, 0, &err);
		node->flags |= INODE_PERSIST;

		blockdev_register_partition(master, node, start, len);
		vfs_icache_put(node);
		i++;
	}
	return 0;
}

int module_exit(void)
{
	return 0;
}

int module_deps(char *b)
{
	return CONFIG_VERSION_NUMBER;
}
