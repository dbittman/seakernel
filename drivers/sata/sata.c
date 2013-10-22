#include <kernel.h>
#include <module.h>
#include <pmap.h>
#include <types.h>
#include <modules/sata.h>
#include <modules/pci.h>
struct pci_device *sata_pci;

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
	return sata;
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

void ahci_initialize_device(struct hba_memory *abar, struct ahci_device *dev)
{
	printk(0, "[sata]: initializing device %d\n", dev->idx);
	ahci_stop_port_command_engine(&abar->ports[dev->idx]);
	
	/* power on, spin up */
	/* initialize state */
	/* map memory */
	/* identify */
	/* create device */
	
	ahci_start_port_command_engine(&abar->ports[dev->idx]);
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
