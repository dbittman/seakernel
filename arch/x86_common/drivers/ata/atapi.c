#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <modules/ata.h>
#include <sea/dm/block.h> 
#include <sea/tm/process.h>

int atapi_pio_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	unsigned long long lba, unsigned char *buffer)
{
	if(rw != READ)
		return 0;
	/* 0xA8 is READ SECTORS command byte. */
	uint8 read_cmd[12] = { 0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	uint8 status;
	int size;
	/* Select drive (only the slave-bit is set) */
	outb (cont->port_cmd_base+REG_DEVICE, dev->id & (1 << 4));      
	ATA_DELAY (cont);       /* 400ns tm_delay */
	outb (cont->port_cmd_base+REG_FEATURES, 0x0);       /* PIO mode */
	outb (cont->port_cmd_base+4, 2048 & 0xFF);
	outb (cont->port_cmd_base+5, 2048 >> 8);
	outb (cont->port_cmd_base+REG_COMMAND, 0xA0);       /* ATA PACKET command */
	while ((status = inb (cont->port_cmd_base+REG_STATUS) & 0x80))     /* BUSY */
		asm ("pause");
	while (!((status = inb (cont->port_cmd_base+REG_STATUS)) & 0x8) && !(status & 0x1))
		asm ("pause");
	/* DRQ or ERROR set */
	if (status & 0x1) {
		size = -1;
		return 0;
	}
	read_cmd[9] = 1;              /* 1 sector */
	read_cmd[2] = (lba >> 0x18) & 0xFF;   /* most sig. byte of LBA */
	read_cmd[3] = (lba >> 0x10) & 0xFF;
	read_cmd[4] = (lba >> 0x08) & 0xFF;
	read_cmd[5] = (lba >> 0x00) & 0xFF;   /* least sig. byte of LBA */
	/* Send ATAPI/SCSI command */
	outsw (cont->port_cmd_base+REG_DATA, (uint16 *) read_cmd, 6);
	/* Wait for IRQ that says the data is ready. */
	// schedule ();
	tm_delay_sleep(6);
	
	/* Read actual size */
	size =
	(((int) inb (cont->port_cmd_base+REG_LBA_HIG)) << 8) |
	(int) (inb (cont->port_cmd_base+REG_LBA_MID));
	/* This example code only supports the case where the data transfer
	* of one sector is done in one step. */
	if (size != 2048)
		return 0;
	/* Read data. */
	again:
	insw (cont->port_cmd_base + REG_DATA, buffer, size / 2);
	
	/* The controller will send another IRQ even though we've read all
	* the data we want.  Wait for it -- so it doesn't interfere with
	* subsequent operations: */
	tm_delay_sleep(6);
	/* Wait for BSY and DRQ to clear, indicating Command Finished */
	while((status = inb (cont->port_cmd_base+REG_STATUS)) & 0x88) 
		;
	cleanup:
	return size;
}

int atapi_rw_main(int rw, int dev, u64 blk_, char *buf)
{
	unsigned long long blk = blk_;
	struct ata_device *device = get_ata_device(dev);
	struct ata_controller *cont = device->controller;
	mutex_acquire(cont->wait);
	if(!(device->flags & F_EXIST)) {
		mutex_release(cont->wait);
		return 0;
	}
	int ret;
	ret = atapi_pio_rw(cont, device, rw, blk, (unsigned char*)buf);
	mutex_release(cont->wait);
	return ret;
}

int atapi_rw_main_multiple(int rw, int dev, u64 blk, char *buf, int num)
{
	int count=0;
	for(int i=0;i<num;i++)
	{
		count += atapi_rw_main(rw, dev, blk + i, buf + (i * 2048));
	}
	return count;
}

int ioctl_atapi(int min, int cmd, long arg)
{
	
	return 0;
}
