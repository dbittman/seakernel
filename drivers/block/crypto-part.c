#include <kernel.h>
#include <sea/dm/block.h>
#include <module.h>
#include <sea/fs/inode.h>
#include <modules/psm.h>
#include <ll.h>

#define MAX_CRYPTO 32
int cp_major;
struct crypt_part {
	int psm_minor;
	int used;
	dev_t dev;
};
struct cp_ioctl_arg {
	char *devname;
	int keylength;
	unsigned char *key;
};
struct crypt_part list[MAX_CRYPTO];

struct crypt_part *crypto_create_dev(dev_t dev)
{
	int i;
	for(i=0;i<MAX_CRYPTO;i++)
	{
		if(!list[i].used) {
			list[i].used = 1;
			break;
		}
	}
	if(i == MAX_CRYPTO)
		return 0;
	
	struct disk_info di;
	di.length=0;
	di.num_sectors=0;
	di.sector_size=512;
	di.flags = PSM_DISK_INFO_NOPART;
	int psm_minor = psm_register_disk_device(PSM_CRYPTO_PART_ID, GETDEV(cp_major, i), &di);
	list[i].psm_minor = psm_minor;
	list[i].dev = dev;
	return &list[i];
}

int cp_rw_multiple(int rw, int min, u64 blk, char *buf, int count)
{
	int i, total=0;
	for(i=0;i<count;i++) {
		/* BUG/TODO: Handle non 512 sector sizes */
		int r = dm_do_block_rw(rw, list[min].dev, blk+i, buf+i*512, 0);
		if(r <= 0) return r;
		total+=r;
	}
	return total;
}

int cp_rw_single(int rw, int min, u64 blk, char *buf)
{
	return dm_do_block_rw(rw, list[min].dev, blk, buf, 0);
}

int cp_ioctl(int min, int cmd, long arg)
{
	struct cp_ioctl_arg *ia = (struct cp_ioctl_arg *)arg;
	if(cmd == 1) {
		struct inode *node = vfs_get_idir(ia->devname, 0);
		if(!node) return -ENOENT;
		printk(1, "[crypt-part]: creating crypto device for %s (dev=%x)\n", ia->devname, node->dev);
		if(crypto_create_dev(node->dev))
			return 0;
	}
	return -EINVAL;
}

int module_install()
{
	memset(list, 0, sizeof(list));
	cp_major = dm_set_available_block_device(cp_rw_single, 512, cp_ioctl, cp_rw_multiple, 0);
	crypto_create_dev(0);
	return 0;
}

int module_exit()
{
	
	
	return 0;
}
