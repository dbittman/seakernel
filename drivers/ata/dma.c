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
	prdtable_t *t = (prdtable_t *)cont->prdt_virt;
	t->addr = cont->dma_buf_phys;
	t->size = size;
	t->last = 0x8000;
	outl(cont->port_bmr_base + BMR_PRDT, (unsigned)cont->prdt_phys);
	if (rw == WRITE)
		memcpy(cont->dma_buf_virt, buffer, size);
	unsigned char st = inb(cont->port_bmr_base + BMR_STATUS);
	st |= BMR_STATUS_ERROR | BMR_STATUS_IRQ;
	outb(cont->port_bmr_base + BMR_STATUS, st);
	return 1;
}

int ata_start_command(struct ata_controller *cont, struct ata_device *dev, 
	unsigned block, char rw, int count)
{
	unsigned long long addr = block;
	unsigned char cmd=0;
	if(rw == READ)
		cmd = 0x25;
	else
		cmd = 0x35;
	outb(cont->port_cmd_base+REG_DEVICE, 0x40 | (dev->id << 4));
	ATA_DELAY(cont);
	outb(cont->port_cmd_base+REG_SEC_CNT, 0x00);
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)(addr >> 24));
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)(addr >> 32));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)(addr >> 40));
	outb(cont->port_cmd_base+REG_SEC_CNT, count/512);
	outb(cont->port_cmd_base+REG_LBA_LOW, (unsigned char)addr);
	outb(cont->port_cmd_base+REG_LBA_MID, (unsigned char)(addr >> 8));
	outb(cont->port_cmd_base+REG_LBA_HIG, (unsigned char)(addr >> 16));
	
	while(1)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY) && (x & STATUS_DRDY))
			break;
	}
	outb(cont->port_cmd_base+REG_COMMAND, cmd);
	return 1;
}

volatile char dma_busy;
int ata_dma_rw(struct ata_controller *cont, struct ata_device *dev, int rw, 
	unsigned blk, char *buf, int count)
{
	unsigned size=512;
	count=1;
	ata_dma_init(cont, dev, size * count, rw, (unsigned char *)buf);
	uint8_t cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg = (rw == READ ? 8 : 0);
	outb(cont->port_bmr_base, cmdReg);
	while(1)
	{
		int x = ata_reg_inb(cont, REG_STATUS);
		if(!(x & STATUS_BSY))
			break;
	}
	ata_start_command(cont, dev, blk, rw, count);
	ATA_DELAY(cont);
	cmdReg = inb(cont->port_bmr_base + BMR_COMMAND);
	cmdReg |= 0x1 | (rw == READ ? 8 : 0);
	cont->irqwait=0;
	outb(cont->port_bmr_base, cmdReg);
	unsigned char st=0;
	while(!cont->irqwait) {
		//wait_flag_except(&cont->irqwait, 0);
		//schedule();
		//__super_sti();
		st = inb(cont->port_bmr_base + BMR_STATUS);
		uint8_t sus = ata_reg_inb(cont, REG_STATUS);
		printk(0, "%d: %x %x %x\n", cont->irqwait, st, sus, !(sus & STATUS_BSY));
		if(st & 0x2)
			panic(PANIC_NOSYNC, "DMA err");
		if(sus & STATUS_ERR)
			panic(PANIC_NOSYNC, "Error in reg_stat");
		if(!(sus & STATUS_BSY))
			break;
		if(!(st & 0x4))
			continue;
	}
	uint8_t status = ata_reg_inb(cont, REG_STATUS);
	if (!(status & STATUS_ERR)) {
		if(st & 0x2)
			goto try_again;
	} else
	{
		try_again:
		printk(1, "[ata]: DMA operation on %d:%d failed (%x %x). Falling back to PIO mode (%d more)\n", 
			cont->id, dev->id, status, st, --cont->dma_use);
		/* Error in the transfer. We assume that the data is junk. Fall back to PIO mode */
		return ata_pio_rw(cont, dev, rw, (unsigned long long)blk, 
			(unsigned char *)buf, 512*count);
	}
	if (rw == READ)
		memcpy(buf, cont->dma_buf_virt, 512);
	
	st = inb(cont->port_bmr_base + BMR_COMMAND);
	outb(cont->port_bmr_base + BMR_COMMAND, st & ~0x1);
	st = inb(cont->port_bmr_base + BMR_STATUS);
	outb(cont->port_bmr_base + BMR_STATUS, st | BMR_STATUS_ERROR | BMR_STATUS_IRQ);
	return 512;
}
