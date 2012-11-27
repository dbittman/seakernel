#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include "ata.h"
extern struct ata_controller *primary, *secondary;

typedef struct {
	unsigned addr;
	unsigned short size;
	unsigned short last;
}__attribute__((packed)) prdtable_t;

/* TODO: Multiple buffers, are we filling the buffer at the right time when we write... */
/* Also, make the partition reading use the defualt mode, not PIO always */
/* fix fsck reporting errors */
/* add time-out */
/* error handling */
int ata_dma_init(struct ata_controller *cont, struct ata_device *dev, 
	int size, int rw, unsigned char *buffer)
{
	int num_entries = (size / (64*1024))+1;
	int i;
	prdtable_t *t = (prdtable_t *)cont->prdt_virt;
	unsigned offset=0;
	for(i=0;i<num_entries;i++) {
		t->addr = cont->dma_buf_phys + offset;
		int this_size = (size-offset);
		if(this_size >= (64*1024)) this_size=0;
		t->size = this_size;
		t->last = 0;
		offset += this_size ? this_size : (64*1024);
		if((i+1) < num_entries) t++;
	}
	assert(offset == (unsigned)size);
	t->last = 0x8000;
	if (rw == WRITE)
		memcpy(cont->dma_buf_virt, buffer, size);
	outl(cont->port_bmr_base + BMR_PRDT, (unsigned)cont->prdt_phys);
	return 1;
}

int ata_start_command(struct ata_controller *cont, struct ata_device *dev, 
	u64 block, char rw, int count)
{
	u64 addr = block;
	unsigned char cmd=0;
	if(rw == READ)
		cmd = 0x25;
	else
		cmd = 0x35;
	outb(cont->port_cmd_base+REG_DEVICE, 0x40 | (dev->id << 4));
	ATA_DELAY(cont);
	
	outb(cont->port_cmd_base+REG_SEC_CNT, (unsigned char)((count >> 8)  & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)((addr  >> 24) & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)((addr  >> 32) & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)((addr  >> 40) & 0xFF));
	outb(cont->port_cmd_base+REG_SEC_CNT, (unsigned char)(        count & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)(         addr & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)((addr  >> 8)  & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)((addr  >> 16) & 0xFF));
	while(1)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY) && (x & STATUS_DRDY))
			break;
	}
	outb(cont->port_cmd_base+REG_COMMAND, cmd);
	return 1;
}

int ata_dma_rw_do(struct ata_controller *cont, struct ata_device *dev, int rw, 
	u64 blk, unsigned char *buf, int count)
{
	unsigned size=512;
	mutex_on(cont->wait);
	
	ata_dma_init(cont, dev, size * count, rw, (unsigned char *)buf);
	
	uint8_t cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg = (rw == READ ? 8 : 0);
	outb(cont->port_bmr_base, cmdReg);
	unsigned char st = inb(cont->port_bmr_base + BMR_STATUS);
	st &= ~BMR_STATUS_ERROR; //clear error bit
	st &= ~BMR_STATUS_IRQ; //clear irq bit
	outb(cont->port_bmr_base + BMR_STATUS, st);
	
	while(1)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY))
			break;
	}
	
	ata_start_command(cont, dev, blk, rw, count);
	
	cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg |= 0x1 | (rw == READ ? 8 : 0);
	cont->irqwait=0;
	int ret = size * count;
	outb(cont->port_bmr_base + BMR_COMMAND, cmdReg);
	while(ret) {
		st = inb(cont->port_bmr_base + BMR_STATUS);
		uint8_t sus = ata_reg_inb(cont, REG_STATUS);
		if(st & 0x2) {
			outb(cont->port_bmr_base + BMR_STATUS, 0x2);
			ret=0;
		}
		if(sus & STATUS_ERR)
			ret=0;
		if(!(sus & STATUS_BSY) && (st & 0x4) && !(st & 0x1))
			break;
	}
	st = inb(cont->port_bmr_base + BMR_COMMAND);
	outb(cont->port_bmr_base + BMR_COMMAND, st & ~0x1);
	
	uint8_t status = ata_reg_inb(cont, REG_STATUS);
	st = inb(cont->port_bmr_base + BMR_STATUS);
	if (rw == READ && ret)
		memcpy(buf, cont->dma_buf_virt, size * count);
	
	if(!ret)
	{
		kprintf("[ata]: dma transfer failed (start=%d, len=%d), resetting...\n", (unsigned)blk, count);
		/* An error occured in the drive - we must issue a drive reset command */
		outb(cont->port_ctl_base, 0x4);
		ATA_DELAY(cont);
		outb(cont->port_ctl_base, 0x0);
		ATA_DELAY(cont);
	}
	
	mutex_off(cont->wait);
	return ret;
}

int ata_dma_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	u64 blk, unsigned char *buf, int count)
{
	return ata_dma_rw_do(cont, dev, rw, blk, buf, count);
}
