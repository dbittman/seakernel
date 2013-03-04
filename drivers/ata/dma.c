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

int ata_dma_init(struct ata_controller *cont, struct ata_device *dev, 
	int size, int rw, unsigned char *buffer)
{
	int num_entries = ((size-1) / (64*1024))+1;
	int i;
	prdtable_t *t = (prdtable_t *)cont->prdt_virt;
	unsigned offset=0;
	if(num_entries >= 512) return -1;
	for(i=0;i<num_entries;i++) {
		if(!(t->addr = cont->dma_buf_phys[i])) {
			unsigned phys, virt;
			if(allocate_dma_buffer(64*1024, &virt, &phys) == -1)
				return -1;
			t->addr = cont->dma_buf_phys[i] = phys;
			cont->dma_buf_virt[i] = virt;
		}
		unsigned this_size = (size-offset);
		if(this_size >= (64*1024)) this_size=0;
		t->size = (unsigned short)this_size;
		t->last = 0;
		if (rw == WRITE)
			memcpy((void *)cont->dma_buf_virt[i], buffer+offset, this_size);
		offset += this_size ? this_size : (64*1024);
		if((i+1) < num_entries) t++;
	}
	assert(offset == (unsigned)size);
	t->last = 0x8000;
	outl(cont->port_bmr_base + BMR_PRDT, (unsigned)cont->prdt_phys);
	return 0;
}

int ata_start_command(struct ata_controller *cont, struct ata_device *dev, 
	u64 block, char rw, unsigned short count)
{
	u64 addr = block;
	unsigned char cmd=0;
	if(rw == READ)
		cmd = (dev->flags & F_LBA48 ? 0x25 : 0xC8);
	else
		cmd = (dev->flags & F_LBA48 ? 0x35 : 0xCA);
	if(!(dev->flags & F_LBA48))
		outb(cont->port_cmd_base+REG_DEVICE, 0xE0 | (dev->id << 4) | ((block >> 24) & 0x0F));
	else
		outb(cont->port_cmd_base+REG_DEVICE, 0x40 | (dev->id << 4));
	ATA_DELAY(cont);
	if(dev->flags & F_LBA48) {
		outb(cont->port_cmd_base+REG_SEC_CNT, (unsigned char)((count >> 8)  & 0xFF));
		outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)((addr  >> 24) & 0xFF));
		outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)((addr  >> 32) & 0xFF));
		outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)((addr  >> 40) & 0xFF));
	}
	outb(cont->port_cmd_base+REG_SEC_CNT, (unsigned char)(        count & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)(         addr & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)((addr  >> 8)  & 0xFF));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)((addr  >> 16) & 0xFF));
	int timeout=100000;
	while(timeout--)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY) && (x & STATUS_DRDY))
			break;
	}
	if(timeout <= 0) {
		printk(4, "[ata]: timeout on waiting for ready on command writing\n");
		return -1;
	}
	outb(cont->port_cmd_base+REG_COMMAND, cmd);
	return 0;
}

int ata_dma_rw_do(struct ata_controller *cont, struct ata_device *dev, int rw, 
	u64 blk, unsigned char *buf, unsigned count)
{
	//pci_write_dword(ata_pci->bus, ata_pci->dev, ata_pci->func, 4, ata_pci->pcs->command);
	unsigned size=512;
	mutex_acquire(cont->wait);
	
	uint8_t cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg = (rw == READ ? 8 : 0);
	outb(cont->port_bmr_base, cmdReg);
	unsigned char st = inb(cont->port_bmr_base + BMR_STATUS);
	st &= ~BMR_STATUS_ERROR; //clear error bit
	st &= ~BMR_STATUS_IRQ; //clear irq bit
	outb(cont->port_bmr_base + BMR_STATUS, st);
	
	int timeout=100000;
	while(timeout--)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY))
			break;
	}
	if(timeout <= 0) {
		mutex_release(cont->wait);
		printk(4, "[ata]: timeout on waiting for ready\n");
		return -EIO;
	}
	if(ata_dma_init(cont, dev, size * count, rw, (unsigned char *)buf) == -1)
	{
		mutex_release(cont->wait);
		printk(4, "[ata]: could not allocate enough dma space for the specified transfer\n");
		return -EIO;
	}
	if(ata_start_command(cont, dev, blk, rw, count) == -1)
	{
		mutex_release(cont->wait);
		printk(4, "[ata]: error in starting command sequence\n");
		return -EIO;
	}
	
	cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg |= 0x1 | (rw == READ ? 8 : 0);
	cont->irqwait=0;
	int ret = size * count;
	sti();
	outb(cont->port_bmr_base + BMR_COMMAND, cmdReg);
	timeout=1000000;
	while(ret && timeout--) {
		if(cont->irqwait) break;
		sti();
		schedule();
		char wst = inb(cont->port_bmr_base + BMR_STATUS);
		if(wst & BMR_STATUS_ERROR)
			ret=0;
		st = inb(cont->port_cmd_base+REG_STATUS);
		if(st & STATUS_ERR || st & STATUS_DF)
			ret=0;
	}
	if(timeout <= 0) {
		mutex_release(cont->wait);
		printk(4, "[ata]: timeout on waiting for data transfer\n");
		return -EIO;
	}
	st = inb(cont->port_bmr_base + BMR_COMMAND);
	outb(cont->port_bmr_base + BMR_COMMAND, st & ~0x1);
	
	st = inb(cont->port_bmr_base + BMR_STATUS);
	if(st & 2)
		ret=0;
	if (rw == READ && ret) {
		int num_entries = ((size-1) / (64*1024))+1;
		int i;
		for(i=0;i<num_entries;i++) {
			int sz;
			if((i+1) == num_entries)
				sz = size*count;
			else
				sz = 64*1024;
			memcpy(buf + i*64*1024, (void *)cont->dma_buf_virt[i], sz);
		}
	}
	if(!ret)
	{
		kprintf("[ata]: dma transfer failed (start=%d, len=%d), resetting...\n", (unsigned)blk, count);
		/* An error occured in the drive - we must issue a drive reset command */
		outb(cont->port_ctl_base, 0x4);
		ATA_DELAY(cont);
		outb(cont->port_ctl_base, 0x0);
		ATA_DELAY(cont);
	}
	
	mutex_release(cont->wait);
	return ret ? ret : -EIO;
}

int ata_dma_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	u64 blk, unsigned char *buf, int count)
{
	if(count >= 128) {
		int i=0;
		int ret=0;
		for(i=0;i<count / 128;i++)
		{
			ret += ata_dma_rw_do(cont, dev, rw, blk + i*128, buf + i*128*512, 128);
		}
		ret += ata_dma_rw_do(cont, dev, rw, blk+i*128, buf + i*128*512, count - i*128);
		return ret;
	}
	return ata_dma_rw_do(cont, dev, rw, blk, buf, count);
}
