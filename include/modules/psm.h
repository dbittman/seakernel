#ifndef __MOD_PSM_H
#define __MOD_PSM_H
#include <config.h>
#if CONFIG_MODULE_PSM
#include <types.h>
#include <dev.h>

#define PSM_DEVICE_MAGIC 0xBEE51E55

struct disk_info {
	uint64_t length;
	uint64_t num_sectors;
	uint64_t sector_size;
	
};

struct part_info {
	int num;
	uint64_t start_lba;
	uint64_t num_sectors;
	uint64_t sysid;
};

struct psm_device {
	uint32_t magic;
	struct disk_info info;
	struct part_info part;
	dev_t dev;
};

#define PSM_AHCI_ID 1

int psm_register_disk_device(int identifier, dev_t dev, struct disk_info *info);
int psm_unregister_disk_device(int identifier, int psm_minor);

void psm_initialize_table();
int psm_table_insert(dev_t dev, struct disk_info *di, struct part_info *pt);
void psm_table_remove(int index);
void psm_table_get(int index, struct psm_device *d);

#endif
#endif
