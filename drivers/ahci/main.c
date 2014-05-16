#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <sea/mm/pmap.h>
#include <sea/mutex.h>
#include <sea/mm/vmm.h>
#include <sea/types.h>
#include <modules/ahci.h>
#include <modules/pci.h>
#include <sea/asm/system.h>
#include <sea/cpu/interrupt.h>
#include <sea/dm/block.h>
#include <sea/loader/symbol.h>
#include <modules/psm.h>
#include <sea/tm/schedule.h>
#include <sea/cpu/interrupt.h>

struct pci_device *ahci_pci;
int ahci_int = 0;
struct hba_memory *hba_mem;
struct pmap *ahci_pmap;
struct ahci_device *ports[32];
int ahci_major=0;
struct pci_device *get_ahci_pci()
{
	struct pci_device *ahci = pci_locate_class(0x1, 0x6);
	if(!ahci) ahci = pci_locate_device(0x8086, 0x8c03);
	if(!ahci) ahci = pci_locate_device(0x8086, 0x2922);
	if(!ahci)
		return 0;
	ahci->flags |= PCI_ENGAGED;
	ahci->flags |= PCI_DRIVEN;
	hba_mem = (void *)(addr_t)ahci->pcs->bar5;
	if(!(ahci->pcs->command & 4))
		printk(KERN_DEBUG, "[ahci]: setting PCI command to bus mastering mode\n");
	unsigned short cmd = ahci->pcs->command | 4;
	ahci->pcs->command = cmd;
	pci_write_dword(ahci->bus, ahci->dev, ahci->func, 4, cmd);
	/* of course, we need to map a virtual address to physical address
	 * for paging to not hate on us... */
	hba_mem = (void *)pmap_get_mapping(ahci_pmap, (addr_t)hba_mem);
	printk(KERN_DEBUG, "[ahci]: mapping hba_mem to %x -> %x\n", hba_mem, ahci->pcs->bar5);
	printk(KERN_DEBUG, "[ahci]: using interrupt %d\n", ahci->pcs->interrupt_line+32);
	ahci_int = ahci->pcs->interrupt_line+32;
	return ahci;
}

void ahci_create_device(struct ahci_device *dev)
{
	dev->created=1;
#if CONFIG_MODULE_PSM
	struct disk_info di;
	di.length=dev->identify.lba48_addressable_sectors*512;
	di.num_sectors=dev->identify.lba48_addressable_sectors;
	di.sector_size=512;
	di.flags = 0;
	dev->psm_minor = psm_register_disk_device(PSM_AHCI_ID, GETDEV(ahci_major, dev->idx), &di);
#endif
}

void ahci_interrupt_handler(registers_t *regs, int int_no)
{
	int i;
	for(i=0;i<32;i++) {
		if(hba_mem->interrupt_status & (1 << i)) {
			hba_mem->ports[i].interrupt_status = ~0;
			hba_mem->interrupt_status = (1 << i);
			ahci_flush_commands((struct hba_port *)&hba_mem->ports[i]);
		}
	}
}

int ahci_port_acquire_slot(struct ahci_device *dev)
{
	while(1) {
		int i;
		mutex_acquire(&dev->lock);
		for(i=0;i<32;i++)
		{
			if(!(dev->slots & (1 << i))) {
				dev->slots |= (1 << i);
				mutex_release(&dev->lock);
				return i;
			}
		}
		mutex_release(&dev->lock);
		tm_schedule();
	}
}

void ahci_port_release_slot(struct ahci_device *dev, int slot)
{
	mutex_acquire(&dev->lock);
	dev->slots &= ~(1 << slot);
	mutex_release(&dev->lock);
}

/* since a DMA transfer must write to contiguous physical RAM, we need to allocate
 * buffers that allow us to create PRDT entries that do not cross a page boundary.
 * That means that each PRDT entry can transfer a maximum of PAGE_SIZE bytes (for
 * 0x1000 page size, that's 8 sectors). Thus, we allocate a buffer that is page aligned, 
 * in a multiple of PAGE_SIZE, so that the PRDT will write to contiguous physical ram
 * (the key here is that the buffer need not be contiguous across multiple PRDT entries).
 */
int ahci_rw_multiple_do(int rw, int min, u64 blk, char *out_buffer, int count)
{
	uint32_t length = count * ATA_SECTOR_SIZE;
	int d = min;
	struct ahci_device *dev = ports[d];
	u64 end_blk = dev->identify.lba48_addressable_sectors;
	if(blk >= end_blk)
		return 0;
	if((blk+count) > end_blk)
		count = end_blk - blk;
	if(!count)
		return 0;
	
	int num_pages = ((ATA_SECTOR_SIZE * (count-1)) / PAGE_SIZE) + 1;
	assert(length <= (unsigned)num_pages * 0x1000);
	unsigned char *buf = kmalloc_a(0x1000 * num_pages);
	int num_read_blocks = count;
	struct hba_port *port = (struct hba_port *)&hba_mem->ports[dev->idx];
	if(rw == WRITE)
		memcpy(buf, out_buffer, length);
	
	int slot=ahci_port_acquire_slot(dev);
	if(!ahci_port_dma_data_transfer(hba_mem, port, dev, slot, rw == WRITE ? 1 : 0, (addr_t)buf, count, blk))
		num_read_blocks = 0;
	
	ahci_port_release_slot(dev, slot);
	
	if(rw == READ && num_read_blocks)
		memcpy(out_buffer, buf, length);
	
	kfree(buf);
	return num_read_blocks * ATA_SECTOR_SIZE;
}

/* and then since there is a maximum transfer amount because of the page size
 * limit, wrap the transfer function to allow for bigger transfers than that even.
 */
int ahci_rw_multiple(int rw, int min, u64 blk, char *out_buffer, int count)
{
	int i=0;
	int ret=0;
	int c = count;
	for(i=0;i<count;i+=(PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE)
	{
		int n = (PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE;
		if(n > c)
			n=c;
		ret += ahci_rw_multiple_do(rw, min, blk+i, out_buffer + ret, n);
		c -= n;
	}
	return ret;
}

int ahci_rw_single(int rw, int dev, u64 blk, char *buf)
{
	return ahci_rw_multiple_do(rw, dev, blk, buf, 1);
}

int ioctl_ahci(int min, int cmd, long arg)
{
	return -EINVAL;
}

int irq1;
int module_install()
{
	printk(KERN_DEBUG, "[ahci]: initializing ahci driver...\n");
	ahci_pmap = pmap_create(0, 0);
	if(!(ahci_pci = get_ahci_pci()))
	{
		printk(KERN_DEBUG, "[ahci]: no AHCI controllers present!\n");
		pmap_destroy(ahci_pmap);
		return -ENOENT;
	}
	irq1 = interrupt_register_handler(ahci_int, ahci_interrupt_handler, 0);
	ahci_major = dm_set_available_block_device(ahci_rw_single, ATA_SECTOR_SIZE, ioctl_ahci, ahci_rw_multiple, 0);
	ahci_init_hba(hba_mem);
	ahci_probe_ports(hba_mem);
	return 0;
}

int module_tm_exit()
{
	int i;
	dm_unregister_block_device(ahci_major);
	interrupt_unregister_handler(ahci_int, irq1);
	for(i=0;i<32;i++)
	{
		if(ports[i]) {
			mutex_destroy(&(ports[i]->lock));
			kfree(ports[i]->clb_virt);
			kfree(ports[i]->fis_virt);
			for(int j=0;j<HBA_COMMAND_HEADER_NUM;j++)
				kfree(ports[i]->ch[j]);
#if CONFIG_MODULE_PSM
			if(ports[i]->created)
				psm_unregister_disk_device(PSM_AHCI_ID, ports[i]->psm_minor);
#endif
			kfree(ports[i]);
		}
	}
	pmap_destroy(ahci_pmap);
	ahci_pci->flags = 0;
	return 0;
}
