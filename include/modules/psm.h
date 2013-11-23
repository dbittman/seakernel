#ifndef __MOD_PSM_H
#define __MOD_PSM_H
#include <config.h>
#if CONFIG_MODULE_PSM
#include <types.h>
#include <dev.h>

struct disk_info {
	u64int_t length;
	u64int_t num_sectors;
	u64int_t sector_size;
	
};

int register_disk_device(int identifier, dev_t dev, struct disk_info *info);
int unregister_disk_device(int identifier, int psm_minor);

#endif
#endif
