#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include "ata.h"
#include <block.h>
void remove_devices()
{
	struct dev_rec *next1;
	while(nodes)
	{
		next1 = nodes->next;
		nodes->node->count=0;
		iremove_force(nodes->node);
		kfree(nodes);
		nodes = next1;
	}
	__a=__b=__c=__d=0;
}

int ata_disk_sync(struct ata_controller *cont)
{
	mutex_on(cont->wait);
	delay_sleep(100);
	mutex_off(cont->wait);
	return 0;
	printk(1, "[ata]: Syncing controller %d\n", cont->id);
	mutex_on(cont->wait);
	outb(cont->port_cmd_base+REG_COMMAND, 0xEA);
	int x = 30000;
	while(--x)
	{
		unsigned char poll = inb(cont->port_cmd_base+REG_STATUS);
		if(poll & STATUS_ERR)
		{
			printk(6, "[ata]: Disk Cache Flush command failed in controller %d\n", 
				cont->id);
			mutex_off(cont->wait);
			return -1;
		}
		if(!(poll & STATUS_BSY))
			break;
		delay_sleep(1);
		printk(1, "%d: %d\n", poll, x);
	}
	if(!x)
	{
		printk(6, "[ata]: Disk Cache Flush command timed out in controller %d\n", 
			cont->id);
		mutex_off(cont->wait);
		return -1;
	}
	delay_sleep(100);
	mutex_off(cont->wait);
	return 0;
}

struct ata_device *get_ata_device(int min, int *part)
{
	int a = min % 4;
	int cont = a / 2;
	int dev = a % 2;
	*part = ((min / 4)-1);
	if(cont)
		return &secondary->devices[dev];
	return &primary->devices[dev];
}

void ata_irq_handler(registers_t regs)
{
	struct ata_controller *cont = (regs.int_no == (32+ATA_PRIMARY_IRQ) ? primary : secondary);
	char st = inb(cont->port_bmr_base + BMR_STATUS);
	if(st & 0x4) {
		cont->irqwait++;
		ata_reg_inb(cont, REG_STATUS);
		inb(cont->port_bmr_base + BMR_STATUS);
		outb(cont->port_bmr_base + BMR_STATUS, 0x4);
	}
}
