#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <modules/ata.h>
#include <sea/dm/block.h>
#include <sea/loader/symbol.h>
#include <modules/psm.h>
#include <sea/mm/dma.h>
int init_ata_device()
{
	struct pci_device *ata = pci_locate_class(0x1, 0x1);
	if(!ata) 
		return 1;
	ata->flags |= PCI_ENGAGED;
	ata->flags |= PCI_DRIVEN;
	unsigned short bmr;
	if(!(bmr = pci_get_base_address(ata)))
	{
		ata->flags = PCI_ERROR;
		return -1;
	}
	/* allow this device bus-mastering mode */
	unsigned short cmd = ata->pcs->command | 4;
	ata->pcs->command = cmd;
	pci_write_dword(ata->bus, ata->dev, ata->func, 4, cmd);
	
	ata_pci = ata;
	primary->port_bmr_base=bmr;
	if(ata->pcs->bar0 == 0 || ata->pcs->bar0 == 1)
		primary->port_cmd_base = ATA_PRIMARY_CMD_BASE;
	else
		primary->port_cmd_base = ata->pcs->bar0;
	if(ata->pcs->bar1 == 0 || ata->pcs->bar1 == 1)
		primary->port_ctl_base = ATA_PRIMARY_CTL_BASE;
	else
		primary->port_ctl_base = ata->pcs->bar1;
	
	primary->irq = ATA_PRIMARY_IRQ;
	primary->id=0;
	
	secondary->port_bmr_base=bmr+0x8;
	
	if(ata->pcs->bar2 == 0 || ata->pcs->bar2 == 1)
		secondary->port_cmd_base = ATA_SECONDARY_CMD_BASE;
	else
		secondary->port_cmd_base = ata->pcs->bar2;
		
	if(ata->pcs->bar3 == 0 || ata->pcs->bar3 == 1)
		secondary->port_ctl_base = ATA_SECONDARY_CTL_BASE;
	else
		secondary->port_ctl_base = ata->pcs->bar3;
	
	secondary->irq = ATA_SECONDARY_IRQ;
	secondary->id=1;
	return 0;
}

void allocate_dma(struct ata_controller *cont)
{
	addr_t buf;
	addr_t p;
	if(!cont->prdt_virt) {
		cont->prdt_dma.p.size = 0x1000;
		cont->prdt_dma.p.alignment = 0x1000;
		mm_allocate_dma_buffer(&cont->prdt_dma);
		cont->prdt_virt = (void *)cont->prdt_dma.v;
		cont->prdt_phys = cont->prdt_dma.p.address;
	}
	for(int i=0;i<512;i++) {
		cont->dma_buffers[i].p.size = 64 * 1024;
		cont->dma_buffers[i].p.alignment = 0x1000;
		cont->dma_buffers[i].p.address = 0;
	}
	int ret = mm_allocate_dma_buffer(&cont->dma_buffers[0]);
	if(ret == -1)
	{
		kprintf("[ata]: could not allocate DMA buffer\n");
		cont->dma_use=0;
		return;
	}
	cont->dma_use=ATA_DMA_ENABLE;
}

int init_ata_controller(struct ata_controller *cont)
{
	cont->enabled=1;
	int i;
	/* we don't need interrupts during the identification */
	for (i = 0; i < 2; i++) {
		ata_reg_outb(cont, REG_DEVICE, DEVICE_DEV(i));
		ATA_DELAY(cont);
		ata_reg_outb(cont, REG_CONTROL, CONTROL_NIEN);
	}
	struct ata_device dev;
	if(cont->port_bmr_base)
		allocate_dma(cont);
	unsigned char in=0;
	unsigned char m, h;
	for (i = 0; i <= 1; i++)
	{
		dev.id=i;
		dev.controller = cont;
		dev.flags=0;
		
		outb(cont->port_cmd_base+REG_DEVICE, i ? 0xB0 : 0xA0);
		ATA_DELAY(cont);
		
		if(!inb(cont->port_cmd_base+REG_STATUS)) {
			printk(2, "[ata]: %d:%d: no status\n", cont->id, i);
			continue;
		}
		
		/* reset the LBA ports, and send the identify command */
		outb(cont->port_cmd_base+REG_LBA_LOW, 0);
		ATA_DELAY(cont);
		outb(cont->port_cmd_base+REG_LBA_MID, 0);
		ATA_DELAY(cont);
		outb(cont->port_cmd_base+REG_LBA_HIG, 0);
		ATA_DELAY(cont);
		outb(cont->port_cmd_base+REG_COMMAND, COMMAND_IDENTIFY);
		ATA_DELAY(cont);
		
		/* wait until we get READY, ABORT, or no device */
		while(1)
		{
			in = inb(cont->port_cmd_base+REG_STATUS);
			if((!(in & STATUS_BSY) && (in & STATUS_DRQ)) || in & STATUS_ERR || !in)
				break;
			ATA_DELAY(cont);
		}
		
		/* no device here. go to the next device */
		if(!in) {
			printk(4, "[ata]: %d:%d: no device\n", cont->id, i);
			continue;
		}
		/* we got an ABORT response. This means that the device is likely
		 * an ATAPI or a SATA drive. */
		if(in & STATUS_ERR)
		{
			ATA_DELAY(cont);
			m=inb(cont->port_cmd_base+REG_LBA_MID);
			ATA_DELAY(cont);
			h=inb(cont->port_cmd_base+REG_LBA_HIG);
			if((m == 0x14 && h == 0xEB))
			{
				dev.flags |= F_ATAPI;
				/* ATAPI devices get the ATAPI IDENTIFY command */
				outb(cont->port_cmd_base+REG_COMMAND, COMMAND_IDENTIFY_ATAPI);
				ATA_DELAY(cont);
			} else if(m==0x3c && h==0xc3) {
				dev.flags |= F_SATA;
			} else if(m == 0x69 && h == 0x96) {
				dev.flags |= F_SATAPI;
			} else {
				printk(2, "[ata]: %d:%d: unknown (%d:%d)\n", cont->id, i, m, h);
				continue;
			}
		}
		
		dev.flags |= F_EXIST;
		/* we need to check if DMA really works... */
		int dma_ok = 1;
		if((dev.flags & F_ATAPI))
			dma_ok=0;
		else
			dev.flags |= F_DMA;
		
		unsigned short tmp[256];
		int idx;
		for (idx = 0; idx < 256; idx++)
			tmp[idx] = inw(cont->port_cmd_base+REG_DATA);
		
		unsigned lba48_is_supported = tmp[83] & 0x400;
		unsigned lba28 = *(unsigned *)(tmp+60);
		unsigned long long lba48 = *(unsigned long long *)(tmp+100);
		
		if(lba48)
			dev.length = lba48;
		else if(lba28) {
			dev.length = lba28;
			dev.flags |= F_LBA28;
		}
		
		/* if we do dma, we need to re-enable interrupts... */
		if(dma_ok) {
			ata_reg_outb(cont, REG_CONTROL, 0);
			dev.flags |= F_DMA;
		}
		printk(2, "[ata]: %d:%d: %s, flags=0x%x, length = %d sectors (%d MB)\n", 
			cont->id, dev.id, 
			dev.flags & F_ATAPI ? "atapi" : (dev.flags & F_SATA ? "sata" : 
			(dev.flags & F_SATAPI ? "satapi" : "pata")),
			dev.flags, (unsigned)dev.length, 
			((unsigned)dev.length/2)/1024);
		
		if(lba48 && !lba48_is_supported)
			printk(2, "[ata]: %d:%d: conflict in lba48 support reporting\n", cont->id, dev.id);
		else
			dev.flags |= F_LBA48;
		
		if(dev.flags & F_LBA28 || dev.flags & F_LBA48)
			dev.flags |= F_ENABLED;
		else
			printk(2, "[ata]: %d:%d: device reports no LBA accessing modes. Disabling this device.\n", cont->id, dev.id);
		memcpy(&cont->devices[i], &dev, sizeof(struct ata_device));
#if CONFIG_MODULE_PSM
		struct disk_info di;
		int size = 512;
		if(dev.flags & F_ATAPI) {
			size = 2048;
			di.flags = PSM_DISK_INFO_NOPART;
		} else
			di.flags = 0;
		di.length=dev.length*size;
		di.num_sectors=dev.length;
		di.sector_size=size;
		
		cont->devices[i].created = 1;
		cont->devices[i].psm_minor = psm_register_disk_device(PSM_ATA_ID, GETDEV((dev.flags & F_ATAPI) ? api : 3, cont->id * 2 + dev.id), &di);
#endif
	}
	return 0;
}
