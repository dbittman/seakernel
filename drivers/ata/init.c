#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include "ata.h"
#include <block.h>
int __a, __b, __c, __d;
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

int create_device(struct ata_controller *cont, struct ata_device *dev, char *node)
{
	int maj=3;
	if(dev->flags & F_ATAPI || dev->flags & F_SATAPI)
		maj = api;
	int a = cont->id * 2 + dev->id;
	char n = 'h';
	int v=0;
	if(dev->flags & F_ATAPI) {
		n = 'c';
		v=__b++;
	}
	else if(dev->flags & F_SATA)
	{
		n = 's';
		v=__c++;
	}
	else if(dev->flags & F_SATAPI) {
		n = 'p';
		v=__d++;
	} else
		v=__a++;
	memset(node, 0, 16);
	sprintf(node, "%cd%c", n, 'a' + v);
	struct inode *i = dfs_cn(node, S_IFBLK, maj, a);
	struct dev_rec *d = nodes;
	struct dev_rec *new = (struct dev_rec *)kmalloc(sizeof(*d));
	new->node=i;
	new->next = d;
	nodes=new;
	return 0;
}

int read_partitions(struct ata_controller *cont, struct ata_device *dev, char *node)
{
	unsigned p = find_kernel_function("enumerate_partitions");
	if(!p)
		return 0;
	int d = 3*256 + cont->id * 2 + dev->id;
	int (*e_p)(int, int, struct partition *);
	e_p = (int (*)(int, int, struct partition *))p;
	struct partition part;
	int i=0;
	while(i<64)
	{
		/* Returns the i'th partition of device 'd' into info struct part. */
		int r = e_p(i, d, &part);
		if(!r)
			break;
		memcpy(&dev->ptable[i], &part, sizeof(part));
		if(dev->ptable[i].sysid)
		{
			printk(0, "[ata]: %d:%d: read partition start=%d, len=%d\n", cont->id, dev->id, dev->ptable[i].start_lba, dev->ptable[i].length);
			int a = cont->id * 2 + dev->id;
			char tmp[17];
			memset(tmp, 0, 17);
			sprintf(tmp, "%s%d", node, i+1);
			struct inode *in = dfs_cn(tmp, S_IFBLK, (dev->flags & F_ATAPI 
				|| dev->flags & F_SATAPI) ? api : 3, a+(i+1)*4);
			struct dev_rec *dr = nodes;
			struct dev_rec *new = (struct dev_rec *)kmalloc(sizeof(*dr));
			new->node=in;
			new->next = dr;
			nodes=new;
		}
		i++;
	}
	
	return 0;
}

extern char ata_dma_buf[];

void allocate_dma(struct ata_controller *cont)
{
	unsigned buf;
	unsigned p;
	if(!cont->prdt_virt) {
		buf = (unsigned)kmalloc_ap(0x1000, &p);
		cont->prdt_virt = (uint64_t *)buf;
		cont->prdt_phys = p;
	}
	if(!cont->dma_buf_virt) {
		int ret = allocate_dma_buffer(64*1024, &buf, &p);
		if(ret == -1)
		{
			kprintf("[ata]: could not allocate DMA buffer\n");
			cont->dma_use=0;
			return;
		}
		cont->dma_buf_virt[0] = (unsigned)buf;
		cont->dma_buf_phys[0] = (unsigned)p;
		cont->dma_use=ATA_DMA_ENABLE;
	}
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
		
		if(!inb(cont->port_cmd_base+REG_STATUS)) continue;
		
		/* reset the LBA ports, and send the identify command */
		outb(cont->port_cmd_base+REG_LBA_LOW, 0);
		outb(cont->port_cmd_base+REG_LBA_MID, 0);
		outb(cont->port_cmd_base+REG_LBA_HIG, 0);
		outb(cont->port_cmd_base+REG_COMMAND, COMMAND_IDENTIFY);
		ATA_DELAY(cont);
		
		/* wait until we get READY, ABORT, or no device */
		while(1)
		{
			in = inb(cont->port_cmd_base+REG_STATUS);
			if((!(in & STATUS_BSY) && (in & STATUS_DRQ)) || in & STATUS_ERR || !in)
				break;
		}
		
		/* no device here. go to the next device */
		if(!in) 
			continue;
		/* we got an ABORT response. This means that the device is likely
		 * an ATAPI or a SATA drive. */
		if(in & STATUS_ERR)
		{
			m=inb(cont->port_cmd_base+REG_LBA_MID);
			h=inb(cont->port_cmd_base+REG_LBA_HIG);
			if((m == 0x14 && h == 0xEB) || (m == 0x69 && h == 0x96))
			{
				dev.flags |= F_ATAPI;
				/* ATAPI devices get the ATAPI IDENTIFY command */
				outb(cont->port_cmd_base+REG_COMMAND, COMMAND_IDENTIFY_ATAPI);
				ATA_DELAY(cont);
			} else
				continue;
		}
		
		dev.flags |= F_EXIST;
		int dma_ok = 1; //TODO
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
			printk(2, "[ata]: %d:%d: conflict in lba48 support reporting\n");
		else
			dev.flags |= F_LBA48;
		
		memcpy(&cont->devices[i], &dev, sizeof(struct ata_device));
		char node[16];
		create_device(cont, &cont->devices[i], node);
		if(!(dev.flags & F_ATAPI) && !(dev.flags & F_SATAPI))
			read_partitions(cont, &cont->devices[i], node);
	}
	return 0;
}
