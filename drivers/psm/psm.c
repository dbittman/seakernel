/* Persistant Storage Manager - psm
 * Provides a common interface for creating device nodes, and
 * devices nodes for partitions, translating partition R/Ws into
 * raw device R/Ws.
 */
#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <sea/dm/block.h>
#include <sea/loader/symbol.h>
#include <sea/dm/block.h>
#include <modules/psm.h>
#include <sea/fs/devfs.h>
#define MAX_PREFIXES 64

int psm_major;

char *disk_prefixes[MAX_PREFIXES] = {
	"i",
	"a",
	"s",
	"u",
	"crypt"
};

void generate_name(int id, dev_t dev, char *name, int part)
{
	memset(name, 0, 16);
	int minor = MINOR(dev);
	if(minor > 25) {
		if(part)
			snprintf(name, 16, "%sd%d_%d", disk_prefixes[id], minor, part);
		else
			snprintf(name, 16, "%sd%d", disk_prefixes[id], minor);
	} else {
		if(part)
			snprintf(name, 16, "%sd%c%d", disk_prefixes[id], 'a'+minor, part);
		else
			snprintf(name, 16, "%sd%c", disk_prefixes[id], 'a'+minor);
	}
}

int read_partition_dev(dev_t dev, struct part_info *pi, int n)
{
	addr_t p = loader_find_kernel_function("part_get_partition");
	if(!p)
		return 0;
	int (*call)(dev_t, struct part_info *, int) = (void *)p;
	return call(dev, pi, n);
}

int psm_enumerate_partitions(int id, dev_t dev, struct disk_info *di)
{
	struct part_info pi;
	int i=0;
	while(read_partition_dev(dev, &pi, i))
	{
		char name[16];
		generate_name(id, dev, name, i+1);
		pi.num = i+1;
		int minor = psm_table_insert(dev, di, &pi, name);
		printk(KERN_DEBUG, "[psm]: register (%d, %d): partition %s\n", MAJOR(dev), MINOR(dev), name);
		struct inode *node = devfs_add(devfs_root, name, S_IFBLK, psm_major, minor);
		psm_table_set_node(minor, node);
		i++;
	}
	return i;
}

int psm_register_disk_device(int identifier, dev_t dev, struct disk_info *info)
{
	char name[16];
	generate_name(identifier, dev, name, 0);
	int minor = psm_table_insert(dev, info, 0, name);
	/* create raw device node */
	printk(KERN_DEBUG, "[psm]: register (%d, %d): %s\n", MAJOR(dev), MINOR(dev), name);
	struct inode *node = devfs_add(devfs_root, name, S_IFBLK, psm_major, minor);
	psm_table_set_node(minor, node);
	/* enumerate partitions and create partitions nodes */
	if(!(info->flags & PSM_DISK_INFO_NOPART))
		psm_enumerate_partitions(identifier, dev, info);
	return minor;
}

int psm_unregister_disk_device(int identifier, int psm_minor)
{
	printk(KERN_INFO, "[psm]: unregister_disk_device: not implemented\n");
	return 0;
}

int psm_do_rw_multiple(int multiple, int rw, int min, u64 blk, char *out_buffer, int count)
{
	struct psm_device d;
	psm_table_get(min, &d);
	if(d.magic != PSM_DEVICE_MAGIC)
		return 0;
	/* adjust for partitions */
	uint32_t part_off=0, part_len=0;
	u64 end_blk = d.info.num_sectors;
	if(d.part.num > 0) {
		part_off = d.part.start_lba;
		part_len = d.part.num_sectors;
		end_blk = part_len + part_off;
	}
	blk += part_off;
	if(end_blk > 0) {
		if(blk >= end_blk)
			return 0;
		if((blk+count) > end_blk)
			count = end_blk - blk;
	}
	if(!count)
		return 0;
	return multiple ? dm_do_block_rw_multiple(rw, d.dev, blk, out_buffer, count, 0) : dm_do_block_rw(rw, d.dev, blk, out_buffer, 0);
}

int psm_rw_multiple(int rw, int min, u64 blk, char *out_buffer, int count)
{
	return psm_do_rw_multiple(1, rw, min, blk, out_buffer, count);
}

int psm_rw_single(int rw, int min, u64 blk, char *buf)
{
	return psm_do_rw_multiple(0, rw, min, blk, buf, 1);
}

int psm_select(int min, int rw)
{
	struct psm_device d;
	psm_table_get(min, &d);
	if(d.magic != PSM_DEVICE_MAGIC)
		return -ENOENT;
	return dm_block_device_select(d.dev, rw);
}

int psm_ioctl(int min, int cmd, long arg)
{
	struct psm_device d;
	psm_table_get(min, &d);
	if(d.magic != PSM_DEVICE_MAGIC)
		return -ENOENT;
	return dm_block_ioctl(d.dev, cmd, arg);
}

int module_install()
{
	printk(KERN_DEBUG, "[psm]: initializing\n");
	psm_initialize_table();
	psm_major = dm_set_available_block_device(psm_rw_single, 512, psm_ioctl, psm_rw_multiple, psm_select);
	loader_add_kernel_symbol(psm_register_disk_device);
	loader_add_kernel_symbol(psm_unregister_disk_device);
	return 0;
}

int module_exit()
{
	dm_unregister_block_device(psm_major);
	loader_remove_kernel_symbol("psm_register_disk_device");
	loader_remove_kernel_symbol("psm_unregister_disk_device");
	psm_table_destroy();
	return 0;
}
