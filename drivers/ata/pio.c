#include <kernel.h>
#include <dev.h>
#include <modules/pci.h>
#include <modules/ata.h>
#include <block.h>
#include <task.h>
#include <sea/cpu/interrupt.h>
int ata_pio_rw(struct ata_controller *cont, struct ata_device *dev, 
	int rw, unsigned long long blk, unsigned char *buffer, unsigned count)
{
	mutex_t *lock = cont->wait;
	mutex_acquire(lock);
	unsigned long long addr = blk;
	char cmd=0;
	char lba48=0;
	if(dev->flags & F_LBA48)
		lba48=1;
	if(rw == READ)
		cmd = 0x20;
	else
		cmd = 0x30;
	cont->irqwait=0;
	if(lba48) {
		cmd += 0x04;
		outb(cont->port_cmd_base+REG_DEVICE, 0x40 | (dev->id << 4));
		ATA_DELAY(cont);
		outb(cont->port_cmd_base+REG_SEC_CNT, 0x00);
		
		outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)(addr >> 24));
		outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)(addr >> 32));
		outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)(addr >> 40));
	}
	
	outb(cont->port_cmd_base+REG_SEC_CNT, count);
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)addr);
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)(addr >> 8));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)(addr >> 16));
	
	if(!lba48)
		outb(cont->port_cmd_base+REG_DEVICE, 0xE0 | (dev->id << 4) 
			| ((addr >> 24) & 0x0F));
	interrupt_set(0);
	
	outb(cont->port_cmd_base+REG_COMMAND, cmd);
	int x = 10000;
	while(--x)
	{
		char poll = inb(cont->port_cmd_base+REG_STATUS);
		if(poll & STATUS_ERR) {
			mutex_release(lock);
			return -EIO;
		}
		if(poll & STATUS_DRQ)
			break;
		ATA_DELAY(cont);
		tm_schedule();
	}
	if(!x) {
		mutex_release(lock);
		return -EIO;
	}
	unsigned idx;
	unsigned tmpword;
	if(rw == READ)
	{
		for (idx = 0; idx < count*256; idx++)
		{
			tmpword = inw(cont->port_cmd_base+REG_DATA);
			buffer[idx * 2] = (unsigned char)tmpword;
			buffer[idx * 2 + 1] = (unsigned char)(tmpword >> 8);
		}
	} else if(rw == WRITE) {
		for (idx = 0; idx < count*256; idx++)
		{
			tmpword = buffer[idx * 2] | (buffer[idx * 2 + 1] << 8);
			asm ("outw %1, %0"::"dN" 
				((short)(cont->port_cmd_base+REG_DATA)), "a" ((short)tmpword));
		}
	}
	mutex_release(lock);
	return count*512;
}
