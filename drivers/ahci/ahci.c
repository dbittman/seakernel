#include <sea/loader/module.h>
#include <modules/ahci.h>
#include <sea/tm/timing.h>
#include <sea/tm/process.h>
#include <sea/mm/dma.h>
#include <sea/cpu/processor.h>
#include <sea/vsprintf.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>

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
		cpu_pause();
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
	tm_thread_delay_sleep(ONE_MILLISECOND);
	/* initialize state */
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
	port->sata_control |= 1;
	tm_thread_delay_sleep(10 * ONE_MILLISECOND);
	port->sata_control |= (~1);
	tm_thread_delay_sleep(10 * ONE_MILLISECOND);
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
	tm_thread_delay_sleep(1 * ONE_MILLISECOND);
	/* initialize state */
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	port->command |= (1 << 28); /* set interface to active */
	port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
	port->sata_control |= 1;
	tm_thread_delay_sleep(10 * ONE_MILLISECOND);
	port->sata_control |= (~1);
	tm_thread_delay_sleep(10 * ONE_MILLISECOND);
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = AHCI_DEFAULT_INT; /* we want some interrupts */
	/* map memory */
	addr_t clb_phys, fis_phys;
	
	dev->dma_clb.p.size = 0x2000;
	dev->dma_clb.p.alignment = 0x1000;
	dev->dma_fis.p.size = 0x1000;
	dev->dma_fis.p.alignment = 0x1000;

	mm_allocate_dma_buffer(&dev->dma_clb);
	mm_allocate_dma_buffer(&dev->dma_fis);

	dev->clb_virt = (void *)dev->dma_clb.v;
	dev->fis_virt = (void *)dev->dma_fis.v;
	clb_phys = dev->dma_clb.p.address;
	fis_phys = dev->dma_fis.p.address;
	dev->slots=0;
	struct hba_command_header *h = (struct hba_command_header *)dev->clb_virt;
	int i;
	for(i=0;i<HBA_COMMAND_HEADER_NUM;i++) {
		dev->ch_dmas[i].p.size = 0x1000;
		dev->ch_dmas[i].p.alignment = 0x1000;
		mm_allocate_dma_buffer(&dev->ch_dmas[i]);
		dev->ch[i] = (void *)dev->ch_dmas[i].v;
		memset(h, 0, sizeof(*h));
		h->command_table_base_l = LOWER32(dev->ch_dmas[i].p.address);
		h->command_table_base_h = UPPER32(dev->ch_dmas[i].p.address);
		h++;
	}
	
	port->command_list_base_l = LOWER32(clb_phys);
	port->command_list_base_h = UPPER32(clb_phys);
	
	port->fis_base_l = LOWER32(fis_phys);
	port->fis_base_h = UPPER32(fis_phys);
 	ahci_start_port_command_engine(port);
	port->sata_error = ~0;
	return ahci_device_identify_ahci(abar, port, dev);
}

uint32_t ahci_check_type(volatile struct hba_port *port)
{
	uint32_t s = port->sata_status;
	printk(2, "[ahci]: port data: sig=%x, stat=%x, ctl=%x, sac=%x\n", port->signature, s, port->command, port->sata_active);
	uint8_t ipm, det;
	ipm = (s >> 8) & 0x0F;
	det = s & 0x0F;
	printk(2, "[ahci]: port check: ipm=%x, det=%x\n", ipm, det);
	if(ipm != 1 || det != 3)
		return 0;
	return port->signature;
}

void ahci_probe_ports(struct hba_memory *abar)
{
	uint32_t pi = abar->port_implemented;
	printk(2, "[ahci]: ports implemented: %x\n", pi);
	int i=0;
	while(i < 32)
	{
		if(pi & 1)
		{
			uint32_t type = ahci_check_type(&abar->ports[i]);
			if(type) {
				printk(2, "[ahci]: detected device on port %d\n", i);
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
		while((abar->bohc & 1) || !(abar->bohc & (1<<1))) cpu_pause();
		printk(KERN_DEBUG, "[ahci]: ownership change completed\n");
	}
	
	/* enable the AHCI and reset it */
	abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	//abar->global_host_control |= HBA_GHC_RESET;
	/* wait for reset to complete */
	//while(abar->global_host_control & HBA_GHC_RESET) cpu_pause();
	/* enable the AHCI and interrupts */
	//abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	abar->global_host_control |= HBA_GHC_INTERRUPT_ENABLE;
	tm_thread_delay_sleep(20 * ONE_MILLISECOND);
	printk(KERN_DEBUG, "[ahci]: caps and ver: %x %x v %x\n", abar->capability, abar->ext_capabilities, abar->version);
}
