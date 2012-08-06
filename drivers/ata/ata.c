#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include "ata.h"
#include <block.h>
struct ata_controller *primary, *secondary;
struct pci_device *ata_pci;
mutex_t *dma_mutex;
int api=0;
struct dev_rec *nodes;

int ata_rw_multiple(int rw, int dev, int blk_, char *buf, int count)
{
	unsigned long long blk = blk_;
	int part;
	struct ata_device *device = get_ata_device(dev, &part);
	struct ata_controller *cont = device->controller;
	if(!(device->flags & F_EXIST)) {
		return 0;
	}
	if(part >= 0) {
		if(blk > (device->ptable[part].length))
			return 0;
		blk += device->ptable[part].start_lba;
	}
	if(blk >= device->length)
		return 0;
	int ret;
	if(cont->dma_use && 0)
		ret = ata_dma_rw(cont, device, rw, blk, buf, count);
	else
		ret = ata_pio_rw(cont, device, rw, blk, (unsigned char *)buf, count);
	return ret;
}

int ata_rw_main(int rw, int dev, int blk_, char *buf)
{
	return ata_rw_multiple(rw, dev, blk_, buf, 1);
}

int ioctl_ata(int min, int cmd, int arg)
{
	int part;
	struct ata_device *device = get_ata_device(min, &part);
	struct ata_controller *cont = device->controller;
	if(!(device->flags & F_EXIST))
		return -ENOENT;
	if(cmd == -1)
	{
		ata_disk_sync(primary);
		ata_disk_sync(secondary);
		return 0;
	}
	if(cmd == 0)
	{
		if(arg)
			*(unsigned *)arg = device->length;
		return 512;
	}
	if(cmd == 1)
	{
		/* Reload partition table */
		mutex_on(primary->wait);
		mutex_on(secondary->wait);
		remove_devices();
		init_ata_controller(primary);
		init_ata_controller(secondary);
		mutex_off(primary->wait);
		mutex_off(secondary->wait);
		return 0;
	}
	if(cmd == -7)
	{
		if(part >= 0) {
			if(arg)
				*(unsigned int *)arg = 512;
			return device->ptable[part].length;
		}
		if(arg)
			*(unsigned int *)arg = 512;
		return device->length;
	}
	return -EINVAL;
}
#include <sys/fcntl.h>
int module_install()
{
	dma_mutex = create_mutex(0);
	dma_busy=0;
	nodes = (struct dev_rec *)kmalloc(sizeof(struct dev_rec));
	api=0;
	__a=__b=__c=__d=0;
	primary   = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	secondary = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	int res = init_ata_device();
	if(res)
	{
		if(res < 0)
			kprintf("Error in init'ing ATA controller\n");
		return EEXIST;
	}
	register_interrupt_handler(32 + ATA_PRIMARY_IRQ, &ata_irq_handler);
	register_interrupt_handler(32 + ATA_SECONDARY_IRQ, &ata_irq_handler2);
	api = set_availablebd(atapi_rw_main, 2048, ioctl_atapi, 0, 0);
	set_blockdevice(3, ata_rw_main, 512, ioctl_ata, ata_rw_multiple, 0);
	primary->wait = create_mutex(0);
	secondary->wait = create_mutex(0);
	init_ata_controller(primary);
	init_ata_controller(secondary);
	return 0;
}

int module_deps(char *b)
{
	write_deps(b, "pci,:");
	return KVERSION;
}

int module_exit()
{
	
	if(api) {
		printk(1, "[ata]: Syncing disks...\n");
		ata_disk_sync(primary);
		ata_disk_sync(secondary);
		delay(100);
		mutex_on(primary->wait);
		mutex_on(secondary->wait);
		remove_devices();
		unregister_block_device(api);
		ata_pci->flags = 0;
		destroy_mutex(primary->wait);
		destroy_mutex(secondary->wait);
		register_interrupt_handler(32 + ATA_PRIMARY_IRQ, 0);
		register_interrupt_handler(32 + ATA_SECONDARY_IRQ, 0);
	}
	kfree(primary);
	kfree(secondary);
	kfree(nodes);
	destroy_mutex(dma_mutex);
	return 0;
}
