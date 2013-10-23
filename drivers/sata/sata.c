#include <kernel.h>
#include <module.h>
#include <pmap.h>
#include <types.h>
#include <modules/sata.h>
#include <modules/pci.h>
struct pci_device *sata_pci;
int ahci_int = 0;
struct hba_memory *hba_mem;
struct pmap *sata_pmap;
struct ahci_device *ports[32];

struct pci_device *get_sata_pci()
{
	struct pci_device *sata = pci_locate_class(0x1, 0x6);
	if(!sata)
		return 0;
	sata->flags |= PCI_ENGAGED;
	sata->flags |= PCI_DRIVEN;
	hba_mem = (void *)sata->pcs->bar5;
	/* of course, we need to map a virtual address to physical address
	 * for paging to not hate on us... */
	hba_mem = (void *)pmap_get_mapping(sata_pmap, (addr_t)hba_mem);
	kprintf("[sata]: mapping hba_mem to %x -> %x\n", hba_mem, sata->pcs->bar5);
	kprintf("[sata]: using interrupt %d\n", sata->pcs->interrupt_line+32);
	ahci_int = sata->pcs->interrupt_line+32;
	return sata;
}

uint32_t ahci_flush_commands(struct hba_port *port)
{
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
}

//char data[512] __attribute__ ((aligned (16)));

void ahci_initialize_device(struct hba_memory *abar, struct ahci_device *dev)
{
	printk(0, "[sata]: initializing device %d\n", dev->idx);
	struct hba_port *port = (struct hba_port *)&abar->ports[dev->idx];
	ahci_stop_port_command_engine(port);
	
	/* power on, spin up */
	port->command |= 6;
	/* initialize state */
	port->interrupt_status = ~0; /* clear pending interrupts */
	port->interrupt_enable = 15; /* we want some interrupts */
	
	port->command |= (1 << 28); /* set interface to active */
	port->command &= ~((1 << 27) | (1 << 26)); /* clear some bits */
	/* TODO: Do we need a delay here? */
	/* map memory */
	addr_t clb_phys, fis_phys;
	void *clb_virt, *fis_virt;
	clb_virt = kmalloc_ap(0x2000, &clb_phys);
	fis_virt = kmalloc_ap(0x1000, &fis_phys);
	dev->clb_virt = clb_virt;
	dev->fis_virt = fis_virt;
	
	struct hba_command_header *h = (struct hba_command_header *)clb_virt;
	int i;
	for(i=0;i<HBA_COMMAND_HEADER_NUM;i++) {
		addr_t phys;
		dev->ch[i] = kmalloc_ap(0x1000, &phys);
		memset(h, 0, sizeof(*h));
		h->command_table_base_l = phys & 0xFFFFFFFF;
		h->command_table_base_h = (phys >> 32) & 0xFFFFFFFF;
		h++;
	}
	
	port->command_list_base_l = (clb_phys & 0xFFFFFFFF);
	port->command_list_base_h = ((clb_phys >> 32) & 0xFFFFFFFF);
	
	port->fis_base_l = (fis_phys & 0xFFFFFFFF);
	port->fis_base_h = ((fis_phys >> 32) & 0xFFFFFFFF);
	
	ahci_start_port_command_engine(port);
	
	/* identify */
	h = (struct hba_command_header *)clb_virt;
	
	h->fis_length = sizeof(struct fis_reg_host_to_device) / 4;
	kprintf("IDENTIFY\n");
	struct hba_command_table *tbl = (struct hba_command_table *)(dev->ch[0]);
	struct fis_reg_host_to_device *fis = (struct fis_reg_host_to_device *)(tbl->command_fis);
	memset(fis, 0, sizeof(*fis));
	fis->fis_type = FIS_TYPE_REG_H2D;
	fis->command = ATA_CMD_IDENTIFY;
	addr_t data_phys;
	volatile short *data = kmalloc_ap(0x1000, &data_phys);
	
	memset(data, 0, 512);
	
	struct hba_prdt_entry *prd = &tbl->prdt_entries[0];
	prd->byte_count = 256 - 1;
	prd->data_base_l = (uint32_t)data_phys;
	prd->data_base_h = 0;
	prd->interrupt_on_complete=0;
	
	h->prdt_len=1;
	
	
	fis->c=1;
	port->command_issue |= 1;
	
	ahci_flush_commands(port);
	kprintf("...ok...\n");
	
	//while(1)
	//{
		printk(0, "%x\n", port->task_file_data);
		asm("pause");
	//}
	
	/* create device */
	for(int j=0;j<128;j++)
	{
		printk(0, "%d: %x\n", j, (unsigned short)data[j]);
	}
	
	
	uint16_t q = *((uint16_t *)(data)+83);
	
	if(q & (1<<10))
		kprintf("LBA48!\n");
	
	//uint64_t l = (*((uint16_t *)(data)+100)<<48) | (*((uint16_t *)(data)+101)<<32) | (*((uint16_t *)(data)+102)<<16) | (*((uint16_t *)(data)+103));
	
	
	uint64_t l = *(unsigned long long *)(data+100);
	
	kprintf("len = %d (%d KB) (%d MB)\n", (uint32_t)l, (uint32_t)l / 1024, ((uint32_t)l/1024)/1024);
	for(;;);
	
}

uint32_t ahci_check_type(volatile struct hba_port *port)
{
	uint32_t s = port->sata_status;
	uint8_t ipm, det;
	ipm = (s >> 8) & 0x0F;
	det = s & 0x0F;
	/* TODO: Where do these numbers come from? */
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
				ahci_initialize_device(abar, ports[i]);
			}
		}
		i++;
		pi >>= 1;
	}
}

void ahci_init_hba(struct hba_memory *abar)
{
	/* enable the AHCI and reset it */
	abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	abar->global_host_control |= HBA_GHC_RESET;
	/* wait for reset to complete */
	while(abar->global_host_control & HBA_GHC_RESET) asm("pause");
	/* enable the AHCI and interrupts */
	abar->global_host_control |= HBA_GHC_AHCI_ENABLE;
	abar->global_host_control |= HBA_GHC_INTERRUPT_ENABLE;
}

void ahci_interrupt_handler()
{
	int i;
	for(i=0;i<32;i++) {
		if(hba_mem->interrupt_status & (1 << i)) {
			kprintf("AHCI interrupt: port %d: %x\n", i, hba_mem->ports[i].interrupt_status);
			hba_mem->ports[i].interrupt_status = ~0;
			hba_mem->interrupt_status = (1 << i);
		}
	}
}

int irq1;
int module_install()
{
	printk(0, "[sata]: initializing sata driver...\n");
	sata_pmap = pmap_create(0, 0);
	if(!(sata_pci = get_sata_pci()))
	{
		printk(0, "[sata]: no AHCI controllers present!\n");
		pmap_destroy(sata_pmap);
		return -ENOENT;
	}
	irq1 = register_interrupt_handler(ahci_int, ahci_interrupt_handler, 0);
	ahci_init_hba(hba_mem);
	ahci_probe_ports(hba_mem);
	return 0;
}

int module_deps(char *b)
{
	write_deps(b, "pci,:");
	return KVERSION;
}

int module_exit()
{
	int i;
	unregister_interrupt_handler(ahci_int, irq1);
	for(i=0;i<32;i++)
	{
		if(ports[i]) {
			mutex_destroy(&(ports[i]->lock));
			kfree(ports[i]);
		}
	}
	pmap_destroy(sata_pmap);
	sata_pci->flags = 0;
	return 0;
}
