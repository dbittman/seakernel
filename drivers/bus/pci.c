/* pci.c - PCI enumeration, init'ing, and driver interfacing. Copyright 2011 Daniel Bittman
*/

/* API:
 * Drivers call pci_locate_device with the vendor and device IDs of the device they drive.
 * This function returns a kmalloc'd structure that contains the BDF and config space of the 
 * device, as well as flags and error values. Good drivers will then look at the flags:
 * 	If the flag has the error flag set, then the device may be malfunctioning - look at the error code
 * 	Otherwise, set the "driven" flag, and start the device. When the device is fully loaded, set
 * 	the "engaged" flag. Drivers should cleanup the flags when the unload.
 */

/* PROC:
 * The pci info is exposed in /proc/pci. In this directory are files named as BDF PCI mappings. e.g. 0.3.1
 * Each file contains the data contained in the pci_list data structure for that device. bus, dev, func, config space, etc.
 * 
 * Each device obviously needs a min value...major and min...yeah. We just use bus*256+dev*8+func. */
#include <kernel.h>
#include <types.h>
#include <pci.h>
#include <mod.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#define PCI_LOGLEVEL 1
struct inode *proc_pci;
volatile int proc_pci_maj;
struct pci_device *pci_list=0;
mutex_t *pci_mutex;
int remove_kernel_symbol(char * unres);
int proc_set_callback(int major, int( *callback)(char rw, struct inode *inode, int m, char *buf, int, int));
struct inode *pfs_cn_node(struct inode *to, char *name, int mode, int major, int minor);
/* Adds a device to the list of devices */
int pci_add_device(struct pci_device *dev)
{
	if(!dev) return 0;
	mutex_on(pci_mutex);
	struct pci_device *tmp = pci_list;
	pci_list = dev;
	dev->next = tmp;
	if(tmp)
		tmp->prev=dev;
	dev->prev=0;
	mutex_off(pci_mutex);
	return 1;
}

/* Removes a device from the linked list */
void pci_remove_device(struct pci_device *dev)
{
	if(!dev) return;
	mutex_on(pci_mutex);
	if(dev->prev)
		dev->prev->next = dev->next;
	if(dev->next)
		dev->next->prev = dev->prev;
	if(pci_list == dev)
		pci_list = dev->prev ? dev->prev : dev->next;
	kfree(dev->pcs);
	iremove_force(dev->node);
	kfree(dev);
	mutex_off(pci_mutex);
}

/* Deletes the entire linked list */
void pci_destroy_list()
{
	struct pci_device *tmp = pci_list;
	while(tmp)
	{
		struct pci_device *next1 = tmp->next;
		pci_remove_device(tmp);
		tmp=next1;
	}
}

static const char * class_code[13] = 
{ 
"Legacy", "Mass Storage Controller", "Network Controller", 
"Video Controller", "Multimedia Unit", "Memory Controller", "Bridge",
"Simple Communications Controller", "Base System Peripheral", "Input Device",
"Docking Station", "Processor", "Serial Bus Controller"
};

static const char * subclass[13][8] = 
{
{ "Legacy", "VGA", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "SCSI", "IDE", "Floppy", "IPI", "RAID", "Other", "Other", "Other" },
{ "Ethernet", "Token ring", "FDDI", "ATM", "Other", "Other", "Other", "Other" },
{ "VGA", "XGA", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "Video", "Audio", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "RAM", "Flash", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "Host", "ISA", "EISA", "MCA", "PCI-PCI", "PCMCIA", "NuBus", "CardBus" },
{ "Serial", "Parallel", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "PIC", "DMA", "PIT", "RTC", "Other", "Other", "Other", "Other" },
{ "Keyboard", "Digitizer", "Mouse", "Other", "Other", "Other", "Other", "Other" },
{ "Generic", "Other", "Other", "Other", "Other", "Other", "Other", "Other" },
{ "386", "486", "Pentium", "Other", "Other", "Other", "Other", "Other" },
{ "Firewire", "ACCESS", "SSA", "USB", "Other", "Other", "Other", "Other" }
};

/* Reads configuration for a device */
unsigned short pci_read_configword(unsigned short bus, unsigned short slot, unsigned short func, unsigned short offset)
{
	unsigned long address;
	unsigned long lbus = (unsigned long)bus;
	unsigned long lslot = (unsigned long)slot;
	unsigned long lfunc = (unsigned long)func;
	unsigned short tmp = 0;
	
	/* create configuration address */
	address = (unsigned long)((lbus << 16) | (lslot << 11) |
	(lfunc << 8) | (offset & 0xfc) | ((uint32_t)0x80000000));
	
	/* write out the address */
	outl(0xCF8, address);
	/* read in the data */
	tmp = (unsigned short)((inl(0xCFC) >> ((offset & 2) * 8)) & 0xffff);
	return (tmp);
}

uint32_t pci_read_dword(const uint16_t bus, const uint16_t dev, const uint16_t func, const uint32_t reg)
{
	outl(0xCF8, 0x80000000L | ((uint32_t)bus << 16) |((uint32_t)dev << 11) |
	((uint32_t)func << 8) | (reg & ~3));
	return inl(0xCFC + (reg & 3));
}
/* Returns a kamlloc'd struct of pci config space */
struct pci_config_space *get_pci_config(int bus, int dev, int func)
{
	/* Get the dword and parse the vendor and device ID */
	unsigned tmp = pci_read_dword(bus, dev, func, 0);
	unsigned short vendor = (tmp & 0xFFFF);
	unsigned short device = ((tmp >> 16) & 0xFFFF);
	struct pci_config_space *pcs=0;
	if(vendor && vendor != 0xFFFF) {
		/* Valid device! Okay, so the config space is 256 bytes long
		 * and we read in dwords: 64 reads should do it.
		 */
		pcs = (struct pci_config_space *)kmalloc(sizeof(struct pci_config_space));
		int i;
		for(i=0;i<64;i+=16)
		{
			*(uint32_t*)((size_t)pcs + i) = pci_read_dword(bus, dev, func, i);
			*(uint32_t*)((size_t)pcs + i + 4) = pci_read_dword(bus, dev, func, i + 4);
			*(uint32_t*)((size_t)pcs + i + 8) = pci_read_dword(bus, dev, func, i + 8);
			*(uint32_t*)((size_t)pcs + i + 12) = pci_read_dword(bus, dev, func, i + 12);
		}
		if(pcs->class_code < 13 && pcs->subclass != 0x80) {
			printk(PCI_LOGLEVEL, "[pci]: [%3.3d:%2.2d:%d] Vendor %4.4x, Device %4.4x: %s %s\n", bus, dev, func, pcs->vendor_id, pcs->device_id, subclass[pcs->class_code][pcs->subclass], class_code[pcs->class_code]);
		
		}
	}
	return pcs;
}
/* Scans the entire PCI bus(es) and compiles a linked list of devices */
void pci_scan()
{
	unsigned short bus, dev, func, tmp;
	for(bus=0;bus<256;bus++)
	{
		for(dev=0;dev<32;dev++)
		{
			for(func = 0; func < 8; func++)
			{
				struct pci_config_space *pcs=0;
				pcs = get_pci_config(bus, dev, func);
				if(pcs) {
					/* Ok, we found a device. Add it to the list */
					struct pci_device *new = (struct pci_device *)kmalloc(sizeof(struct pci_device));
					new->bus=bus;
					new->dev=dev;
					new->func=func;
					new->pcs=pcs;
					pci_add_device(new);
					char name[64];
					int min=0;
					min = 256*bus + dev*8 + func;
					sprintf(name, "%x.%x.%x", bus, dev, func);
					if(proc_pci_maj) (new->node=pfs_cn_node(proc_pci, name, S_IFREG, proc_pci_maj, min));
					if(new->node)
						new->node->len=sizeof(struct pci_device);
				}
			}
		}
	}
}

/* Search for a pci device based on vendor and device ID */
struct pci_device *pci_locate_device(unsigned short vendor, unsigned short device)
{
	if(vendor == 0xFFFF || device == 0xFFFF)
		return 0;
	struct pci_device *tmp = pci_list;
	while(tmp)
	{
		if(tmp->pcs->vendor_id == vendor && tmp->pcs->device_id == device)
			return tmp;
		tmp=tmp->next;
	}
	return 0;
}

/* Search for a pci device based on vendor and device ID */
struct pci_device *pci_locate_devices(unsigned short vendor, unsigned short device, int i)
{
	if(vendor == 0xFFFF || device == 0xFFFF)
		return 0;
	struct pci_device *tmp = pci_list;
	while(tmp)
	{
		if(!i && tmp->pcs->vendor_id == vendor && tmp->pcs->device_id == device)
			return tmp;
		tmp=tmp->next;
		i--;
	}
	return 0;
}

struct pci_device *pci_locate_class(unsigned short class, unsigned short _subclass)
{
	if(class == 0xFFFF || _subclass == 0xFFFF)
		return 0;
	struct pci_device *tmp = pci_list;
	while(tmp)
	{
		if(tmp->pcs->class_code == class && tmp->pcs->subclass == _subclass)
			return tmp;
		tmp=tmp->next;
	}
	return 0;
}

unsigned pci_get_base_address(struct pci_device *device)
{
	size_t tmp, i;
	
	for(i = 0; i < 6; i++)
	{
		if(*(uint32_t*)(&device->pcs->bar0 + i))break;
	}
	if(i >= 6)
		return 0;
	tmp = *(uint32_t*)(&device->pcs->bar0 + i);
	if(device->pcs->vendor_id == 0x0000 || device->pcs->vendor_id == 0xFFFF)
	{
		kprintf("PCI: Invalid configuration space for get_base_address() in [%3.3d:%2.2d:%d]\n", device->bus, device->dev, device->func);
		return 0;
	}
	
	/* MMIO */
	if(!(tmp & 0x01))
		return tmp & 0xFFFFFFF0;
	
	/* PIO */
	return tmp & 0xFFFFFFFC;
}

int pci_proc_call(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
	int c=0;
	if(rw == READ)
	{
		int bus, dev, func;
		bus = m / 256;
		dev = (m % 256) / 8;
		func = (m % 256) % 8;
		struct pci_device *tmp = pci_list;
		while(tmp)
		{
			if(tmp->bus == bus && tmp->dev  == dev && tmp->func == func)
				break;
			tmp=tmp->next;
		}
		if(tmp) {
			c += proc_append_buffer(buf, (void *)tmp, c, sizeof(struct pci_device), off, len);
			memcpy(buf, (unsigned char *)tmp, sizeof(struct pci_device));
		}
		return c;
	}
	return 0;
}

int module_install()
{
	pci_list=0;
	pci_mutex = create_mutex(0);
	proc_pci_maj = proc_get_major();
	proc_pci=pfs_cn("pci", S_IFDIR, proc_pci_maj, 0);
	proc_set_callback(proc_pci_maj, pci_proc_call);
	printk(1, "[pci]: Scanning pci bus\n");
	pci_scan();
	
	add_kernel_symbol(pci_locate_device);
	add_kernel_symbol(pci_locate_devices);
	add_kernel_symbol(pci_locate_class);
	add_kernel_symbol(pci_get_base_address);
	
	return 0;
}

int module_exit()
{
	pci_destroy_list();
	iremove_force(proc_pci);
	proc_set_callback(proc_pci_maj, 0);
	remove_kernel_symbol("pci_locate_device");
	remove_kernel_symbol("pci_locate_devices");
	remove_kernel_symbol("pci_locate_class");
	remove_kernel_symbol("pci_get_base_address");
	destroy_mutex(pci_mutex);
	return 0;
}
int module_deps(char *b)
{
	return KVERSION;
}
