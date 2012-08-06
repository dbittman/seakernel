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
	bmr = ata->pcs->bar4;
	ata_pci = ata;
	primary->port_bmr_base=bmr;
	primary->port_cmd_base = ATA_PRIMARY_CMD_BASE;
	primary->port_ctl_base = ATA_PRIMARY_CTL_BASE;
	primary->irq = ATA_PRIMARY_IRQ;
	primary->id=0;
	
	secondary->port_bmr_base=bmr;
	secondary->port_cmd_base = ATA_SECONDARY_CMD_BASE;
	secondary->port_ctl_base = ATA_SECONDARY_CTL_BASE;
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

int identify_atapi()
{
	return 0;
}

int init_ata_controller(struct ata_controller *cont){
	cont->irqwait=1;
	int i;
	if(ATA_DMA_ENABLE) {
		/* We poll for PIO, so we don't really need IRQs if we don't have dma */
		for (i = 0; i < 2; i++) {
			ata_reg_outb(cont, REG_DEVICE, DEVICE_DEV(i));
			ATA_DELAY(cont);
			ata_reg_outb(cont, REG_CONTROL, CONTROL_NIEN);
		}
	}
	struct ata_device dev;
	for (i = 0; i <= 1; i++)
	{
		dev.id=i;
		dev.controller = cont;
		dev.flags=0;
		outb(cont->port_cmd_base+REG_DEVICE, 0xA0 | (i ? 0x10 : 0));//id==1 -> slave
		ATA_DELAY(cont);
		outb(cont->port_cmd_base+REG_SEC_CNT, 0);
		outb(cont->port_cmd_base+REG_LBA_LOW, 0);
		outb(cont->port_cmd_base+REG_LBA_MID, 0);
		outb(cont->port_cmd_base+REG_LBA_HIG, 0);
		outb(cont->port_cmd_base+REG_COMMAND, COMMAND_IDENTIFY);
		unsigned char in=0;
		unsigned char m, h;
		while(1)
		{
			in = inb(cont->port_cmd_base+REG_STATUS);
			if((m=inb(cont->port_cmd_base+REG_LBA_MID)) 
					|| (h=inb(cont->port_cmd_base+REG_LBA_HIG)))
				break;
			if(!in)
			{
				cont->devices[i].flags=0;
				break;
			}
			if(!(in & STATUS_BSY) && (in & STATUS_DRQ))
				break;
			if(in & STATUS_ERR)
				break;
		}
		if(!in || in & STATUS_ERR)
		{
			if(m == 0x14)
			{
				printk(3, "[ata]: Found an ATAPI device: ");
				dev.flags |= F_ATAPI;
			} else
			{
				cont->devices[i].flags=0;
				continue;
			}
		} else
			printk(3, "[ata]: Found an ATA device: ");
		printk(3, "%d:%d", cont->id, i);
		dev.flags |= F_EXIST;
		if(!(dev.flags & F_ATAPI)) {
		unsigned short tmp[256];
		int idx;
		for (idx = 0; idx < 256; idx++)
		{
			tmp[idx] = inw(cont->port_cmd_base+REG_DATA);
		}
		if(tmp[83] & 0x400) {
			printk(2, "\tlba48");
			dev.flags |= F_LBA48;
		}
		unsigned lba28 = *(unsigned *)(tmp+60);
		unsigned long long lba48 = *(unsigned long long *)(tmp+100);
		if(!lba28 && !lba48)
		{
			cont->devices[i].flags=0;
			printk(2, "\n");
			continue;
		}
		if(lba48)
			dev.length = lba48;
		if(lba28 && !lba48)
		{
			dev.length = lba28;
			dev.flags |= F_LBA28;
		}
		printk(2, "\tLength = %d sectors (%d MB)\n", (unsigned)dev.length, 
			((unsigned)dev.length/2)/1024);
		} else
		{
			identify_atapi();
			dev.length=~0;
			dev.flags |= F_LBA28;
			printk(2, "\n");
		}
		memcpy(&cont->devices[i], &dev, sizeof(struct ata_device));
		char node[16];
		create_device(cont, &cont->devices[i], node);
		read_partitions(cont, &cont->devices[i], node);
	}
	
	if (cont->port_bmr_base && 0) {
		unsigned buf;
		unsigned p;
		if(!cont->prdt_virt) {
			buf = (unsigned)kmalloc_ap(0x1000, &p);
			cont->prdt_virt = (uint64_t *)buf;
			cont->prdt_phys = p;
		}
		if(!cont->dma_buf_virt) {
			buf = (unsigned)kmalloc_ap(ATA_DMA_MAXSIZE, &p);
			cont->dma_buf_virt = (void *)buf;
			cont->dma_buf_phys = p;
			cont->dma_use=ATA_DMA_ENABLE;
		}
	}
	return 0;
}
