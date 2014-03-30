#include <sea/kernel.h>
#include <sea/loader/module.h>
#include <modules/ahci.h>
#include <sea/tm/schedule.h>

struct hba_command_header *ahci_initialize_command_header(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int write, int atapi, int prd_entries, int fis_len)
{
	struct hba_command_header *h = (struct hba_command_header *)dev->clb_virt;
	h += slot;
	h->write=write ? 1 : 0;
	h->prdb_count=0;
	h->atapi=atapi ? 1 : 0;
	h->fis_length = fis_len;
	h->prdt_len=prd_entries;
	h->prefetchable=0;
	h->bist=0;
	h->pmport=0;
	h->reset=0;
	return h;
}

struct fis_reg_host_to_device *ahci_initialize_fis_host_to_device(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int cmdctl, int ata_command)
{
	struct hba_command_table *tbl = (struct hba_command_table *)(dev->ch[slot]);
	struct fis_reg_host_to_device *fis = (struct fis_reg_host_to_device *)(tbl->command_fis);
	
	memset(fis, 0, sizeof(*fis));
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->command = ata_command;
	fis->c=cmdctl?1:0;
	return fis;
}

void ahci_send_command(struct hba_port *port, int slot)
{
	port->interrupt_status = ~0;
	port->command_issue = (1 << slot);
	ahci_flush_commands(port);
}

int ahci_write_prdt(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int offset, int length, addr_t virt_buffer)
{
	int num_entries = ((length-1) / PRDT_MAX_COUNT) + 1;
	struct hba_command_table *tbl = (struct hba_command_table *)(dev->ch[slot]);
	int i;
	struct hba_prdt_entry *prd;
	for(i=0;i<num_entries-1;i++)
	{
		addr_t phys_buffer = mm_vm_get_map(virt_buffer, 0, 0);
		prd = &tbl->prdt_entries[i+offset];
		prd->byte_count = PRDT_MAX_COUNT-1;
		prd->data_base_l = phys_buffer & 0xFFFFFFFF;
		prd->data_base_h = UPPER32(phys_buffer);
		prd->interrupt_on_complete=0;
		
		length -= PRDT_MAX_COUNT;
		virt_buffer += PRDT_MAX_COUNT;
	}
	addr_t phys_buffer = mm_vm_get_map(virt_buffer, 0, 0);
	prd = &tbl->prdt_entries[i+offset];
	prd->byte_count = length-1;
	prd->data_base_l = phys_buffer & 0xFFFFFFFF;
	prd->data_base_h = UPPER32(phys_buffer);
	prd->interrupt_on_complete=0;
	
	return num_entries;
}

int ahci_port_dma_data_transfer(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot, int write, addr_t virt_buffer, int sectors, uint64_t lba)
{
	int timeout;
	int fis_len = sizeof(struct fis_reg_host_to_device) / 4;
	int ne = ahci_write_prdt(abar, port, dev, slot, 0, ATA_SECTOR_SIZE * sectors, virt_buffer);
	struct hba_command_header *h = ahci_initialize_command_header(abar, port, dev, slot, write, 0, ne, fis_len);
	struct fis_reg_host_to_device *fis = ahci_initialize_fis_host_to_device(abar, port, dev, slot, 1, write ? ATA_CMD_WRITE_DMA_EX : ATA_CMD_READ_DMA_EX);
	fis->device = 1<<6;
	fis->count_l = sectors & 0xFF;
	fis->count_h = (sectors >> 8) & 0xFF;
	
	fis->lba0 = (unsigned char)( lba        & 0xFF);
	fis->lba1 = (unsigned char)((lba >> 8)  & 0xFF);
	fis->lba2 = (unsigned char)((lba >> 16) & 0xFF);
	fis->lba3 = (unsigned char)((lba >> 24) & 0xFF);
	fis->lba4 = (unsigned char)((lba >> 32) & 0xFF);
	fis->lba5 = (unsigned char)((lba >> 40) & 0xFF);
	port->sata_error = ~0;
	timeout = ATA_TFD_TIMEOUT;
	while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
	{
		tm_schedule();
	}
	if(!timeout) goto port_hung;
	
	ahci_send_command(port, slot);
	port->sata_error = ~0;
	timeout = ATA_TFD_TIMEOUT;
	while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
	{
		tm_schedule();
	}
	if(!timeout) goto port_hung;
	
	timeout = AHCI_CMD_TIMEOUT;
	while(--timeout)
	{
		if(!((port->sata_active | port->command_issue) & (1 << slot)))
			break;
		tm_schedule();
	}
	if(!timeout) goto port_hung;
	if(port->sata_error)
	{
		printk(KERN_DEBUG, "[ahci]: device %d: ahci error\n", dev->idx);
		goto error;
	}
	if(port->task_file_data & ATA_DEV_ERR)
	{
		printk(KERN_DEBUG, "[ahci]: device %d: task file data error\n", dev->idx);
		goto error;
	}
	return 1;
	port_hung:
	printk(KERN_DEBUG, "[ahci]: device %d: port hung\n", dev->idx);
	error:
	printk(KERN_DEBUG, "[ahci]: device %d: tfd=%x, serr=%x\n", dev->idx, port->task_file_data, port->sata_error);
	ahci_reset_device(abar, port, dev);
	return 0;
}

int ahci_device_identify_ahci(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev)
{
	int fis_len = sizeof(struct fis_reg_host_to_device) / 4;
	unsigned short *buf = kmalloc_a(0x1000);
	ahci_write_prdt(abar, port, dev, 0, 0, 512, (addr_t)buf);
	struct hba_command_header *h = ahci_initialize_command_header(abar, port, dev, 0, 0, 0, 1, fis_len);
	struct fis_reg_host_to_device *fis = ahci_initialize_fis_host_to_device(abar, port, dev, 0, 1, ATA_CMD_IDENTIFY);
	int timeout = ATA_TFD_TIMEOUT;
	port->sata_error = ~0;
	while ((port->task_file_data & (ATA_DEV_BUSY | ATA_DEV_DRQ)) && --timeout)
		asm("pause");
	if(!timeout)
	{
		printk(KERN_DEBUG, "[ahci]: device %d: identify 1: port hung\n", dev->idx);
		printk(KERN_DEBUG, "[ahci]: device %d: identify 1: tfd=%x, serr=%x\n", dev->idx, port->task_file_data, port->sata_error);
		return 0;
	}
	ahci_send_command(port, 0);
	timeout = AHCI_CMD_TIMEOUT;
	while(--timeout)
	{
		if(!((port->sata_active | port->command_issue) & 1))
			break;
	}
	if(!timeout)
	{
		printk(KERN_DEBUG, "[ahci]: device %d: identify 2: port hung\n", dev->idx);
		printk(KERN_DEBUG, "[ahci]: device %d: identify 2: tfd=%x, serr=%x\n", dev->idx, port->task_file_data, port->sata_error);
		return 0;
	}
	memcpy(&dev->identify, buf, sizeof(struct ata_identify));
	kfree(buf);
	printk(2, "[ahci]: device %d: num sectors=%d: %x, %x\n", dev->idx, dev->identify.lba48_addressable_sectors, dev->identify.ss_2);
	return 1;
}
