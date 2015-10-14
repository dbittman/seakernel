#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <modules/ata.h>
#include <sea/dm/block.h>
#include <sea/sys/fcntl.h>
#include <sea/loader/module.h>
#include <modules/psm.h>
#include <sea/cpu/interrupt.h>
#include <sea/cpu/registers.h>
struct ata_controller *primary, *secondary;
struct pci_device *ata_pci;
int api=0;
struct dev_rec *nodes;
int irq_primary, irq_secondary;
int ata_rw_multiple(int rw, int dev, u64 blk, char *buf, int count)
{
	if(!count) return 0;
	struct ata_device *device = get_ata_device(dev);
	struct ata_controller *cont = device->controller;
	if(!cont->enabled || !(device->flags & F_ENABLED)) 
		return 0;
	if(!(device->flags & F_EXIST))
		return 0;
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
	struct ata_device *device = get_ata_device(min);
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
		if(arg)
			*(unsigned int *)arg = 512;
		return device->length;
	}
	return -EINVAL;
}
int ata_disk_sync_nowait(struct ata_controller *cont);
int module_install(void)
{
	nodes=0;
	api=0;
	primary   = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	secondary = (struct ata_controller *)kmalloc(sizeof(struct ata_controller));
	int res = init_ata_device();
	if(res)
	{
		if(res < 0)
			kprintf("Error in init'ing ATA controller\n");
		kfree(primary);
		kfree(secondary);
		if(nodes) kfree(nodes);
		return EEXIST;
	}
	irq_primary = cpu_interrupt_register_handler(32 + ATA_PRIMARY_IRQ, ata_irq_handler);
	irq_secondary = cpu_interrupt_register_handler(32 + ATA_SECONDARY_IRQ, ata_irq_handler);
	api = dm_set_available_block_device(atapi_rw_main, 2048, ioctl_atapi, atapi_rw_main_multiple, 0);
	dm_set_block_device(3, ata_rw_main, 512, ioctl_ata, ata_rw_multiple, 0);
	primary->wait   = mutex_create(0, 0);
	secondary->wait = mutex_create(0, 0);
	primary->id=0;
	secondary->id=1;
	init_ata_controller(primary);
	init_ata_controller(secondary);
	return 0;
}

int module_exit(void)
{
	if(api) {
		printk(1, "[ata]: Syncing disks...\n");
		ata_disk_sync_nowait(primary);
		ata_disk_sync_nowait(secondary);
		mutex_acquire(primary->wait);
		mutex_acquire(secondary->wait);
#if CONFIG_MODULE_PSM
		if(primary->devices[0].created)
			psm_unregister_disk_device(PSM_ATA_ID, primary->devices[0].psm_minor);
		if(primary->devices[1].created)
			psm_unregister_disk_device(PSM_ATA_ID, primary->devices[1].psm_minor);
		if(secondary->devices[0].created)
			psm_unregister_disk_device(PSM_ATA_ID, secondary->devices[0].psm_minor);
		if(secondary->devices[1].created)
			psm_unregister_disk_device(PSM_ATA_ID, secondary->devices[1].psm_minor);
#endif
		dm_unregister_block_device(api);
		ata_pci->flags = 0;
		int i;
		for(i=0;i<512;i++) {
			if(primary->dma_buffers[i].p.address) {
				mm_free_dma_buffer(&primary->dma_buffers[i]);
				mm_free_dma_buffer(&secondary->dma_buffers[i]);
			}
		}
		if(primary->prdt_virt)
			mm_free_dma_buffer(&primary->prdt_dma);
		if(secondary->prdt_virt)
			mm_free_dma_buffer(&secondary->prdt_dma);
		mutex_destroy(primary->wait);
		mutex_destroy(secondary->wait);
		cpu_interrupt_unregister_handler(32 + ATA_PRIMARY_IRQ, irq_primary);
		cpu_interrupt_unregister_handler(32 + ATA_SECONDARY_IRQ, irq_secondary);
	}
	kfree(primary);
	kfree(secondary);
	kfree(nodes);
	return 0;
}
