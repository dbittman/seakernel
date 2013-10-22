#include <kernel.h>
#include <module.h>
#include <modules/sata.h>
#include <modules/pci.h>
struct pci_device *sata_pci;

struct pci_device *get_sata_pci()
{
	struct pci_device *sata = pci_locate_class(0x1, 0x6);
	if(!sata)
		return 0;
	sata->flags |= PCI_ENGAGED;
	sata->flags |= PCI_DRIVEN;
	
	kprintf("sata: AHCI base memory: %x\n", sata->pcs->bar5);
	
	return sata;
}

int module_install()
{
	printk(0, "[sata]: initializing sata driver...\n");
	if(!(sata_pci = get_sata_pci()))
	{
		printk(0, "[sata]: no AHCI controllers present!\n");
		return -1;
	}
	return 0;
}

int module_deps(char *b)
{
	write_deps(b, "pci,:");
	return KVERSION;
}

int module_exit()
{
	
	return 0;
}
