#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include "ata.h"
#include <block.h>
#include <sys/fcntl.h>
struct ata_controller *primary, *secondary;
struct pci_device *ata_pci;
int api=0;
struct dev_rec *nodes;
int irq1=0, irq2=0;
int ata_rw_multiple(int rw, int dev, u64 blk, char *buf, int count)
{
	int part;
	if(!count) return 0;
	struct ata_device *device = get_ata_device(dev, &part);
	struct ata_controller *cont = device->controller;
	if(!cont->enabled || !(device->flags & F_ENABLED)) 
		return 0;
	if(!(device->flags & F_EXIST))
		return 0;
	if(part >= 0) {
		if(blk+count > (device->ptable[part].length))
			return 0;
		blk += device->ptable[part].start_lba;
	}
	if(blk+count > device->length)
		return 0;
	int ret;
	if(device->flags & F_DMA && cont->dma_use && ATA_DMA_ENABLE)
		ret = ata_dma_rw(cont, device, rw, blk, (unsigned char *)buf, count);
	else
		ret = ata_pio_rw(cont, device, rw, blk, (unsigned char *)buf, count);
	return ret;
}

int ata_rw_main(int rw, int dev, u64 blk, char *buf)
{
	return ata_rw_multiple(rw, dev, blk, buf, 1);
}

int ioctl_ata(int min, int cmd, long arg)
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
		kprintf("[ata]: reload partition tables - not implemented\n");
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

int module_install()
{
	nodes=0;
	api=0;
	__a=__b=__c=__d=0;
	primary   = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	secondary = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	int res = init_ata_device();
	if(res)
	{
		if(res < 0)
			kprintf("Error in init'ing ATA controller\n");
		kfree(primary);
		kfree(secondary);
		kfree(nodes);
		return EEXIST;
	}
	irq1 = register_interrupt_handler(32 + ATA_PRIMARY_IRQ, &ata_irq_handler, 0);
	irq2 = register_interrupt_handler(32 + ATA_SECONDARY_IRQ, &ata_irq_handler, 0);
	api = set_availablebd(atapi_rw_main, 2048, ioctl_atapi, 0, 0);
	set_blockdevice(3, ata_rw_main, 512, ioctl_ata, ata_rw_multiple, 0);
	primary->wait = mutex_create(0, 0);
	secondary->wait = mutex_create(0, 0);
	primary->id=0;
	secondary->id=1;
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
		mutex_acquire(primary->wait);
		mutex_acquire(secondary->wait);
		remove_devices();
		unregister_block_device(api);
		ata_pci->flags = 0;
		mutex_destroy(primary->wait);
		mutex_destroy(secondary->wait);
		unregister_interrupt_handler(32 + ATA_PRIMARY_IRQ, irq1);
		unregister_interrupt_handler(32 + ATA_SECONDARY_IRQ, irq2);
	}
	kfree(primary);
	kfree(secondary);
	kfree(nodes);
	return 0;
}
