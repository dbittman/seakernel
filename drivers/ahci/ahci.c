#include <kernel.h>
#include <sea/loader/module.h>
#include <modules/ahci.h>
#include <sea/tm/schedule.h>
#include <sea/tm/process.h>

uint32_t ahci_flush_commands(struct hba_port *port)
{
	/* the commands may not take effect until the command
	 * register is read again by software, because reasons.
	 */
	uint32_t c = port->command;
	c=c;
	return c;
}

void ahci_stop_port_command_engine(volatile struct hba_port *port)
{
	port->command &= ~HBA_PxCMD_ST;
	port->command &= ~HBA_PxCMD_FRE;
	while((port->command & HBA_PxCMD_CR) || (port->command & HBA_PxCMD_FR))
		asm("pause");
}

void ahci_start_port_command_engine(volatile struct hba_port *port)
{
	while(port->command & HBA_PxCMD_CR);
	port->command |= HBA_PxCMD_FRE;
	port->command |= HBA_PxCMD_ST; 
	ahci_flush_commands((struct hba_port *)port);
}

void ahci_reset_device(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev)
{
	/* TODO: This needs to clear out old commands and lock properly so that new commands can't get sent
	 * while the device is resetting */
	printk(KERN_DEBUG, "[ahci]: device %d: sending COMRESET and reinitializing\n", dev->idx);
	ahci_stop_port_command_engine(port);
	port->sata_error = ~0;
	/* power on, spin up */
	port->command |= 2;
	port->command |= 4;
	ahci_flush_commands(port);
	tm_delay_sleep(1);
	/* initialize state */
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
	port->sata_control |= 1;
	tm_delay_sleep(10);
	port->sata_control |= (~1);
	tm_delay_sleep(10);
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	ahci_start_port_command_engine(port);
	dev->slots=0;
	port->sata_error = ~0;
}

uint32_t ahci_get_previous_byte_count(struct hba_memory *abar, struct hba_port *port, struct ahci_device *dev, int slot)
{
	struct hba_command_header *h = (struct hba_command_header *)dev->clb_virt;
	h += slot;
	return h->prdb_count;
}

int ahci_initialize_device(struct hba_memory *abar, struct ahci_device *dev)
{
	printk(KERN_DEBUG, "[ahci]: initializing device %d\n", dev->idx);
	struct hba_port *port = (struct hba_port *)&abar->ports[dev->idx];
	ahci_stop_port_command_engine(port);
	port->sata_error = ~0;
	/* power on, spin up */
	port->command |= 2;
	port->command |= 4;
	ahci_flush_commands(port);
	tm_delay_sleep(1);
	/* initialize state */
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	port->command |= (1 << 28); /* set interface to active */
	port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
	port->sata_control |= 1;
	tm_delay_sleep(10);
	port->sata_control |= (~1);
	tm_delay_sleep(10);
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	/* map memory */
	addr_t clb_phys, fis_phys;
	void *clb_virt, *fis_virt;
	clb_virt = kmalloc_ap(0x2000, &clb_phys);
	fis_virt = kmalloc_ap(0x1000, &fis_phys);
	dev->clb_virt = clb_virt;
	dev->fis_virt = fis_virt;
	dev->slots=0;
	struct hba_command_header *h = (struct hba_command_header *)clb_virt;
	int i;
	for(i=0;i<HBA_COMMAND_HEADER_NUM;i++) {
		addr_t phys;
		dev->ch[i] = kmalloc_ap(0x1000, &phys);
		memset(h, 0, sizeof(*h));
		h->command_table_base_l = phys & 0xFFFFFFFF;
		h->command_table_base_h = UPPER32(phys);
		h++;
	}
	
	port->command_list_base_l = (clb_phys & 0xFFFFFFFF);
	port->command_list_base_h = UPPER32(clb_phys);
	
	port->fis_base_l = (fis_phys & 0xFFFFFFFF);
	port->fis_base_h = UPPER32(fis_phys);
 	ahci_start_port_command_engine(port);
	port->sata_error = ~0;
	return ahci_device_identify_ahci(abar, port, dev);
}

uint32_t ahci_check_type(volatile struct hba_port *port)
{
	uint32_t s = port->sata_status;
	uint8_t ipm, det;
	ipm = (s >> 8) & 0x0F;
	det = s & 0x0F;
	if(ipm != 1 || det != 3)
		return 0;
	return port->signature;
}

void ahci_probe_ports(struct hba_memory *abar)
{
	uint32_t pi = abar->port_implemented;
	int i=0;
	while(i < 32)
	{
		if(pi & 1)
		{
			uint32_t type = ahci_check_type(&abar->ports[i]);
			if(type) {
				ports[i] = kmalloc(sizeof(struct ahci_device));
				ports[i]->type = type;
				ports[i]->idx = i;
				mutex_create(&(ports[i]->lock), 0);
				if(ahci_initialize_device(abar, ports[i]))
					ahci_create_device(ports[i]);
				else
					printk(KERN_DEBUG, "[ahci]: failed to initialize device %d, disabling port\n", i);
			}
		}
		i++;
		pi >>= 1;
	}
}

void ahci_init_hba(struct hba_memory *abar)
{
	if(abar->ext_capabilities & 1) {
		/* request BIOS/OS ownership handoff */
		printk(KERN_DEBUG, "[ahci]: requesting AHCI ownership change\n");
		abar->bohc |= (1 << 1);
		while((abar->bohc & 1) || !(abar->bohc & (1<<1))) asm("pause");
		printk(KERN_DEBUG, "[ahci]: ownership change completed\n");
	}
	
	/* enable the AHCI and reset it */
	abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	abar->global_host_control |= HBA_GHC_RESET;
	/* wait for reset to complete */
	while(abar->global_host_control & HBA_GHC_RESET) asm("pause");
	/* enable the AHCI and interrupts */
	abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	abar->global_host_control |= HBA_GHC_INTERRUPT_ENABLE;
	tm_delay_sleep(10);
	printk(KERN_DEBUG, "[ahci]: caps and ver: %x %x v %x\n", abar->capability, abar->ext_capabilities, abar->version);
}
