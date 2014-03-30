#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <modules/ata.h>
#include <sea/dm/block.h>
#include <sea/cpu/atomic.h>

int ata_disk_sync(struct ata_controller *cont)
{
	return 0;
	printk(1, "[ata]: Syncing controller %d\n", cont->id);
	mutex_acquire(cont->wait);
	outb(cont->port_cmd_base+REG_COMMAND, 0xEA);
	int x = 30000;
	while(--x)
	{
		unsigned char poll = inb(cont->port_cmd_base+REG_STATUS);
		if(poll & STATUS_ERR)
		{
			printk(6, "[ata]: Disk Cache Flush command failed in controller %d\n", 
				cont->id);
			mutex_release(cont->wait);
			return -1;
		}
		if(!(poll & STATUS_BSY))
			break;
	}
	if(!x)
	{
		printk(6, "[ata]: Disk Cache Flush command timed out in controller %d\n", 
			cont->id);
		mutex_release(cont->wait);
		return -1;
	}
	mutex_release(cont->wait);
	return 0;
}

int ata_disk_sync_nowait(struct ata_controller *cont)
{
	printk(1, "[ata]: Syncing controller %d\n", cont->id);
	outb(cont->port_cmd_base+REG_COMMAND, 0xEA);
	return 0;
}

struct ata_device *get_ata_device(int min)
{
	int cont = min / 2;
	int dev = min % 2;
	if(cont)
		return &secondary->devices[dev];
	return &primary->devices[dev];
}

void ata_irq_handler(registers_t *regs)
{
	struct ata_controller *cont = (regs->int_no == (32+ATA_PRIMARY_IRQ) ? primary : secondary);
	char st = inb(cont->port_bmr_base + BMR_STATUS);
	if(st & 0x4) {
		add_atomic(&cont->irqwait, 1);
		ata_reg_inb(cont, REG_STATUS);
		outb(cont->port_bmr_base + BMR_STATUS, 0x4);
	}
}
