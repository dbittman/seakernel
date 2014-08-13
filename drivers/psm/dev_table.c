#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <sea/mutex.h>
#include <sea/fs/devfs.h>
#include <modules/psm.h>
#include <sea/mm/kmalloc.h>

struct psm_device *table;
int table_length;
int table_index;
mutex_t *table_lock;
void psm_initialize_table()
{
	table_length=8;
	table_index=0;
	table = kmalloc(sizeof(struct psm_device)*8);
	table_lock = mutex_create(0, 0);
}

void psm_table_destroy()
{
	mutex_acquire(table_lock);
	for(int i=0;i<table_index;i++)
	{
		if(table[i].magic == PSM_DEVICE_MAGIC)
		{
			devfs_remove(table[i].node);
		}
	}
	kfree(table);
	mutex_destroy(table_lock);
}

int psm_table_insert(dev_t dev, struct disk_info *di, struct part_info *pt, char *name)
{
	mutex_acquire(table_lock);
	if(table_index == table_length)
	{
		table_length *= 2;
		struct psm_device *newtable = kmalloc(sizeof(struct psm_device) * table_length);
		memcpy(newtable, table, sizeof(struct psm_device)*table_index);
		kfree(table);
		table = newtable;
	}
	table[table_index].magic = PSM_DEVICE_MAGIC;
	table[table_index].dev = dev;
	memcpy(&table[table_index].info, di, sizeof(struct disk_info));
	if(pt) memcpy(&table[table_index].part, pt, sizeof(struct part_info));
	else   memset(&table[table_index].part, 0, sizeof(struct part_info));
	table[table_index].node = 0;
	int ret = table_index++;
	mutex_release(table_lock);
	return ret;
}

void psm_table_set_node(int index, struct inode *n)
{
	mutex_acquire(table_lock);
	table[index].node=n;
	mutex_release(table_lock);
}

void psm_table_remove(int index)
{
	mutex_acquire(table_lock);
	table[index].magic=0;
	mutex_release(table_lock);
}

void psm_table_get(int index, struct psm_device *d)
{
	mutex_acquire(table_lock);
	memcpy(d, &table[index], sizeof(struct psm_device));
	mutex_release(table_lock);
}
