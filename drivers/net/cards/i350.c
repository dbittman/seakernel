#include <kernel.h>
#include <module.h>
#include <modules/pci.h>
#include <pmap.h>

int i350_int;
struct pci_device *i350;

struct pci_device *get_i350_pci()
{
	i350 = pci_locate_device(0x8086, 0x1521);
	if(!i350)
		return 0;
	i350->flags |= PCI_ENGAGED;
	i350->flags |= PCI_DRIVEN;
	printk(KERN_DEBUG, "[i350]: found i350 device, initializing...\n");
	/* of course, we need to map a virtual address to physical address
	 * for paging to not hate on us... */
	//hba_mem = (void *)pmap_get_mapping(i350_pmap, (addr_t)hba_mem);
	i350_int = i350->pcs->interrupt_line+32;
	return i350;
}

int module_install()
{
	
	get_i350_pci();
	
	return 0;
}

int module_exit()
{
	return 0;
}
