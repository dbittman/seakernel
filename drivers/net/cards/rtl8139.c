#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include <mod.h>
#include <char.h>
int rtl8139_maj=-1, rtl8139_min=0;
typedef struct rtl8139_dev_s
{
	unsigned addr, inter, tx_o, rx_o;
	struct pci_device *device;
	struct inode *node;
	
	char *rec_buf;
	
	struct rtl8139_dev_s *next, *prev;
} rtl8139dev_t;

volatile rtl8139dev_t *cards;

rtl8139dev_t *create_new_device(unsigned addr, struct pci_device *device)
{
	rtl8139dev_t *d = (rtl8139dev_t *)kmalloc(sizeof(rtl8139dev_t));
	d->addr = addr;
	d->device = device;
	rtl8139dev_t *t = (rtl8139dev_t *)cards;
	cards = d;
	d->next = t;
	if(t)
		t->prev=d;
	
	return d;
}

void delete_device(rtl8139dev_t *d)
{
	if(d->prev)
		d->prev->next = d->next;
	if(d->next)
		d->next->prev = d->prev;
	if(d == cards)
		cards = d->next;
	kfree(d->rec_buf);
	kfree(d);
}

int rtl8139_reset(int base_addr)
{
        unsigned timeout=10;
        char done=0;
        while(1) {
                outb(base_addr+0x50, inb(base_addr+0x50)|(0x03 << 6));
                outb(base_addr+0x52, 0x00);
                delay(10);
                if(done)
                        break;
                outb(base_addr+0x37, 0x10);
                while(--timeout){
                        delay(10);
                        if(!(inb(base_addr+0x37)&(1<<4)))
                                break;
                }
                if(timeout < 1) {
                        printk(KERN_WARN, "RTL8139: An error occured\n");
                        return 0;
                }
                done++;
        }
        outw(base_addr+0x62, 0x3300);
        if((inb(base_addr+0x52) & 0x04) == 0)
                printk(KERN_WARN, "Warning - RTL8139: PIO not enabled\n");
        timeout=10;
        while(--timeout)
        {
                if((inw(base_addr+0x64) & 0x2C) == 0x2C)
                        break;
                delay(50);
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
	
	dev->rec_buf = (char *)kmalloc(0x10000);
	
	outb(dev->addr+0x50, 0xC0);
	
	// enable Rx and Tx
	outb(dev->addr+0x37, 0x08 | 0x04);
	
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
	outw(dev->addr+0x3C, 0x3);
	outw(dev->addr+0x3E, 0xffff);
	return 0;
}

int recieve(rtl8139dev_t *dev)
{
	printk(1, "[rtl]: TRACE: Recieve packet\n");
	return 0;
}

int rtl8139_int(registers_t *regs)
{
	printk(1, "[rtl]: TRACE: Got irq\n");
	while(1)
	{
		unsigned short data = inw(0x3E);
		if((data&0xF) == 0)
			break;
		if(data&0x01)
		{
			rtl8139dev_t *t = (rtl8139dev_t *)cards;
			while(t) {
				if(t->inter == regs->int_no)
					recieve(t);
				t=t->next;
			}
		}
		else if((data & 0x4) == 0) {
			printk(1, "[rtl]: Device error: %x\n", data);
			rtl8139dev_t *t = (rtl8139dev_t *)cards;
			while(t) {
				if(t->inter == regs->int_no)
					rtl8139_reset(t->addr);
				t=t->next;
			}
		}
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
	rtl8139dev_t *dev = create_new_device(addr, device);
	printk(1, "[rtl8139]: Initiating rtl8139 controller (%x.%x.%x)...\n", device->bus, device->dev, device->func);
	if(rtl8139_init(dev))
		ret++;
	
	if(ret){
		printk(1, "[rtl8139]: Device error when trying to initialize\n");
		device->flags |= PCI_ERROR;
		return -1;
	}
	struct inode *i = dfs_cn("rtl8139", S_IFCHR, rtl8139_maj, rtl8139_min++);
	dev->node = i;
	printk(1, "[rtl8139]: Success!\n");
	device->flags |= PCI_ENGAGED;
	device->flags |= PCI_DRIVEN;
	dev->inter = device->pcs->interrupt_line;
	register_interrupt_handler(dev->inter, (isr_t)&rtl8139_int);
	return 0;
}

int rtl8139_unload_device_pci(rtl8139dev_t *dev)
{
	if(!dev) return 0;
	struct pci_device *device = dev->device;
	printk(1, "[rtl8139]: Unloading device (%x.%x.%x)\n", device->bus, device->dev, device->func);
	device->flags &= ~PCI_ENGAGED;
	device->flags &= ~PCI_DRIVEN;
	iremove_force(dev->node);
	unregister_interrupt_handler(dev->inter, (isr_t)&rtl8139_int);
	delete_device(dev);
	return 0;
}

int rtl8139_rw_main(int rw, int min, char *buf, int count)
{
	return 0;
}

int ioctl_rtl8139(int min, int cmd, int arg)
{
	return 0;
}

int module_install()
{
	rtl8139_min=0;
	cards=0;
	rtl8139_maj = set_availablecd(rtl8139_rw_main, ioctl_rtl8139, 0);
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

int module_deps(char *b)
{
	write_deps(b, "pci,ethernet,:");
	return KVERSION;
}

int module_exit()
{
	printk(1, "[rtl8139]: Shutting down all cards...\n");
	while(cards) /* this call updates 'cards' within it. */
		rtl8139_unload_device_pci((rtl8139dev_t *)cards);
	unregister_char_device(rtl8139_maj);
	return 0;
}
