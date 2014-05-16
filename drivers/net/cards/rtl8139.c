#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <sea/loader/symbol.h>
#include <sea/dm/char.h>
#include <sea/loader/module.h>
#include <sea/fs/devfs.h>
#include <sea/mm/dma.h>

int rtl8139_maj=-1;
typedef struct rtl8139_dev_s
{
	unsigned addr, inter, tx_o, rx_o;
	addr_t rec_buf, rec_buf_virt;
	int inter_id;
	struct pci_device *device;
	struct inode *node;
} rtl8139dev_t;

rtl8139dev_t *rtldev;

rtl8139dev_t *create_new_device(unsigned addr, struct pci_device *device)
{
	rtl8139dev_t *d = (rtl8139dev_t *)kmalloc(sizeof(rtl8139dev_t));
	d->addr = addr;
	d->device = device;
	return d;
}

int rtl8139_reset(int base_addr)
{
	unsigned timeout=10;
	outb(base_addr+0x52, 0x00);
	char done=0;
	while(1) {
			//outb(base_addr+0x50, inb(base_addr+0x50)|(0x03 << 6));
			
			tm_delay(10);
			if(done)
					break;
			outb(base_addr+0x37, 0x10);
			while(--timeout){
					tm_delay(10);
					if(!(inb(base_addr+0x37)&(1<<4)))
							break;
			}
			if(timeout < 1) {
					printk(KERN_WARN, "RTL8139: An error occured\n");
					return 0;
			}
			done++;
	}
	//outw(base_addr+0x62, 0x3300);
	//if((inb(base_addr+0x52) & 0x04) == 0)
	//		printk(KERN_WARN, "Warning - RTL8139: PIO not enabled\n");
	timeout=10;
	while(--timeout)
	{
			if((inw(base_addr+0x64) & 0x2C) == 0x2C)
					break;
			tm_delay(50);
	}
	if(timeout < 1) {
			printk(KERN_WARN, "RTL8139: An error occured\n");
			return 0;
	}
	return 1;
}

enum {
	RTL_RXCFG_FTH_NONE = 0xE000,// No FIFO treshhold
	RTL_RXCFG_RBLN_64K = 0x1800,// 64K Rx Buffer length
	RTL_RXCFG_MDMA_UNLM = 0x700,// Unlimited DMA Burst size
	RTL_RXCFG_AR = 0x10,        // Accept Runt packets
	RTL_RXCFG_AB = 0x08,        // Accept Broadcast packets
	RTL_RXCFG_AM = 0x04,        // Accept Multicast packets
	RTL_RXCFG_APM = 0x02,       // Accept Physical Match packets
	RTL_RXCFG_AAP = 0x01,       // Accept All packets
	RTL_TXCFG_MDMA_1K = 0x600,  // 1K DMA Burst
	RTL_TXCFG_MDMA_2K = 0x700,  // 2K DMA Burst
	RTL_TXCFG_RR_48 = 0x20,     // 48 (16 + 2 * 16) Tx Retry count
};

int rtl8139_init(rtl8139dev_t *dev)
{
	if(!rtl8139_reset(dev->addr))
		return -1;
	addr_t buf, p;
	int ret = mm_allocate_dma_buffer(64*1024, &buf, &p);
	if(ret == -1) {
		printk(0, "[rtl8139]: failed to allocate dma buffer\n");
		return -1;
	}
	dev->rec_buf = p;
	dev->rec_buf_virt = buf;
	outb(dev->addr+0x50, 0xC0);
	
	// get the card out of low power mode
	outb(dev->addr+0x52, 0);
	
	// write the RxBuffer's address
	outl(dev->addr+0x30, (unsigned)dev->rec_buf);
	
	// no missed packets
	outb(dev->addr+0x4C, 0);
	
	// BMCR options
	outw(dev->addr+0x62, 0x2000 | 0x1000 | 0x100);
	
	// MSR options
	outb(dev->addr+0x58, 0x40);
	
	// write rx and tx config
	outl(dev->addr+0x44, RTL_RXCFG_FTH_NONE | RTL_RXCFG_RBLN_64K | 
			RTL_RXCFG_MDMA_UNLM | RTL_RXCFG_AR | RTL_RXCFG_AB | 
			RTL_RXCFG_AM | RTL_RXCFG_APM | RTL_RXCFG_AAP);
	outl(dev->addr+0x40, RTL_TXCFG_MDMA_2K | RTL_TXCFG_RR_48);
	
	// write multicast addresses
	outl(dev->addr+8, 0xFFFFFFFF);
	outl(dev->addr+12, 0xFFFFFFFF);
	
	// lock BCMR registers
	outb(dev->addr+0x50, 0x00);
	
	// enable Rx and Tx
	outb(dev->addr+0x37, 0x08 | 0x04);
	
	// enable all good irqs
	outw(dev->addr+0x3C, 15);
	outw(dev->addr+0x3E, 0xffff);
	return 0;
}

int recieve(rtl8139dev_t *dev, unsigned short data)
{
	printk(1, "[rtl]: TRACE: Recieve packet\n");
	
	return 0;
}


int rtl8139_int_1(registers_t *regs, int int_no)
{
	return 0;
}

int rtl8139_int(registers_t *regs, int int_no)
{ 
	rtl8139dev_t *t=rtldev;
	if((t->inter+IRQ0) == int_no)
	{
		printk(1, "[rtl]: TRACE: Got irq (%d) %x\n", int_no, t->addr);
		unsigned short data = inw(t->addr + 0x3E);
		if(data&0x01)
			recieve(t, data);
		/* ACK */
		outw(t->addr + 0x3E, data);
	}
	return 0;
}

int rtl8139_load_device_pci(struct pci_device *device)
{
	int addr;
	char ret=0;
	if(!(addr = pci_get_base_address(device)))
	{
		device->flags |= PCI_ERROR;
		return 1;
	}
	/* set PCI busmastering */
	unsigned short cmd = device->pcs->command | 4;
	device->pcs->command = cmd;
	pci_write_dword(device->bus, device->dev, device->func, 4, cmd);
	
	rtl8139dev_t *dev = create_new_device(addr, device);
	printk(1, "[rtl8139]: Initiating rtl8139 controller (%x.%x.%x)...\n", 
		device->bus, device->dev, device->func);
	if(rtl8139_init(dev))
		ret++;
	
	if(ret){
		kfree(dev);
		printk(1, "[rtl8139]: Device error when trying to initialize\n");
		device->flags |= PCI_ERROR;
		return -1;
	}
	
	struct inode *i = devfs_add(devfs_root, "rtl8139", S_IFCHR, rtl8139_maj, 0);
	dev->node = i;
	device->flags |= PCI_ENGAGED;
	device->flags |= PCI_DRIVEN;
	dev->inter = device->pcs->interrupt_line;
	dev->inter_id = interrupt_register_handler(dev->inter + IRQ0, (isr_t)rtl8139_int_1, (isr_t)&rtl8139_int);
	printk(1, "[rtl8139]: registered interrupt line %d\n", dev->inter);
	printk(1, "[rtl8139]: Success!\n");
	rtldev = dev;
	return 0;
}

int rtl8139_unload_device_pci(rtl8139dev_t *dev)
{
	if(!dev) return 0;
	struct pci_device *device = dev->device;
	printk(1, "[rtl8139]: Unloading device (%x.%x.%x)\n", device->bus, 
		device->dev, device->func);
	device->flags &= ~PCI_ENGAGED;
	device->flags &= ~PCI_DRIVEN;
	devfs_remove(dev->node);
	interrupt_unregister_handler(dev->inter, dev->inter_id);
	return 0;
}

int rtl8139_rw_main(int rw, int min, char *buf, unsigned int count)
{
	return 0;
}

int ioctl_rtl8139(int min, int cmd, long int arg)
{
	return 0;
}

int module_install()
{
	rtl8139_maj = dm_set_available_char_device(rtl8139_rw_main, ioctl_rtl8139, 0);
	int i=0;
	printk(1, "[rtl8139]: Scanning PCI bus...\n");
	while(1) {
		struct pci_device *dev = pci_locate_devices(0x10ec, 0x8139, i);
		if(!dev)
			break;
		rtl8139_load_device_pci(dev);
		i++;
	}
	return 0;
}

int module_tm_exit()
{
	dm_unregister_char_device(rtl8139_maj);
	rtl8139_unload_device_pci(rtldev);
	return 0;
}
