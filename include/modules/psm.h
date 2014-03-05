#ifndef __MOD_PSM_H
#define __MOD_PSM_H
#include <config.h>
#if CONFIG_MODULE_PSM
#include <types.h>
#include <dev.h>
#include <fs.h>
#define PSM_DEVICE_MAGIC 0xBEE51E55

#define PSM_DISK_INFO_NOPART 1

struct disk_info {
	uint64_t length;
	uint64_t num_sectors;
	uint64_t sector_size;
	uint32_t flags;
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
	struct inode *node;
	dev_t dev;
};

#define PSM_CRYPTO_PART_ID 4
#define PSM_AHCI_ID 1
#define PSM_ATA_ID  0

int psm_register_disk_device(int identifier, dev_t dev, struct disk_info *info);
int psm_unregister_disk_device(int identifier, int psm_minor);

void psm_initialize_table();
void psm_table_destroy();
int psm_table_insert(dev_t dev, struct disk_info *di, struct part_info *pt, char *);
void psm_table_remove(int index);
void psm_table_get(int index, struct psm_device *d);
void psm_table_set_node(int index, struct inode *n);

#endif
#endif
