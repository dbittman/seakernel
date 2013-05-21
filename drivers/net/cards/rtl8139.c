#include <kernel.h>
#include <dev.h>
#include <pci.h>
#include <mod.h>
#include <char.h>
#include <ll.h>

int rtl8139_maj=-1, rtl8139_min=0;
typedef struct rtl8139_dev_s
{
	unsigned addr, inter, tx_o, rx_o;
	struct pci_device *device;
	struct inode *node;
	
	char *rec_buf;
} rtl8139dev_t;
struct llist *rtl_cards;

rtl8139dev_t *create_new_device(unsigned addr, struct pci_device *device)
{
	rtl8139dev_t *d = (rtl8139dev_t *)kmalloc(sizeof(rtl8139dev_t));
	d->addr = addr;
	d->device = device;
	ll_insert(rtl_cards, d);
	return d;
}

int rtl8139_reset(int base_addr)
{
	unsigned timeout=10;
	outb(base_addr+0x52, 0x00);
	char done=0;
	while(1) {
			//outb(base_addr+0x50, inb(base_addr+0x50)|(0x03 << 6));
			
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
	//outw(base_addr+0x62, 0x3300);
	//if((inb(base_addr+0x52) & 0x04) == 0)
	//		printk(KERN_WARN, "Warning - RTL8139: PIO not enabled\n");
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
char rx_buf[0x2000 + 16];
char tx_buf[0x1000];
int rtl8139_init(rtl8139dev_t *dev)
{
	if(!rtl8139_reset(dev->addr))
		return -1;
	dev->rec_buf = rx_buf;
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
	outw(dev->addr+0x3C, 15);
	outw(dev->addr+0x3E, 0xffff);
	return 0;
}

int recieve(rtl8139dev_t *dev, unsigned short data)
{
	printk(1, "[rtl]: TRACE: Recieve packet\n");
	outw(dev->addr + 0x3E, data);
	return 0;
}

int rtl8139_int(registers_t *regs)
{ 
	rtl8139dev_t *t=0;
	struct llistnode *cur;
	rwlock_acquire(&rtl_cards->rwl, RWL_READER);
	ll_for_each_entry(rtl_cards, cur, rtl8139dev_t *, t);
	{
		if((t->inter+IRQ0) == regs->int_no)
		{
			printk(1, "[rtl]: TRACE: Got irq (%d) %x\n", regs->int_no, t->addr);
			unsigned short data = inw(t->addr + 0x3E);
			if(data&0x01)
				recieve(t, data);
		}
	}
	rwlock_release(&rtl_cards->rwl, RWL_READER);
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
	printk(1, "[rtl8139]: Initiating rtl8139 controller (%x.%x.%x)...\n", 
		device->bus, device->dev, device->func);
	if(rtl8139_init(dev))
		ret++;
	
	if(ret){
		kfree(dev->rec_buf);
		ll_remove_entry(rtl_cards, dev);
		kfree(dev);
		printk(1, "[rtl8139]: Device error when trying to initialize\n");
		device->flags |= PCI_ERROR;
		return -1;
	}
	
	//unsigned short cmd = device->pcs->command | 4;
	//device->pcs->command = cmd;
	//pci_write_dword(device->bus, device->dev, device->func, 4, cmd);
	
	struct inode *i = devfs_add(devfs_root, "rtl8139", S_IFCHR, rtl8139_maj, rtl8139_min++);
	dev->node = i;
	device->flags |= PCI_ENGAGED;
	device->flags |= PCI_DRIVEN;
	dev->inter = device->pcs->interrupt_line;
	register_interrupt_handler(dev->inter + IRQ0, (isr_t)&rtl8139_int);
	printk(1, "[rtl8139]: registered interrupt line %d\n", dev->inter);
	printk(1, "[rtl8139]: Success!\n");
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
	unregister_interrupt_handler(dev->inter, (isr_t)&rtl8139_int);
	return 0;
}

int rtl8139_rw_main(int rw, int min, char *buf, unsigned int count)
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
	rtl_cards = ll_create(0);
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
	unregister_char_device(rtl8139_maj);
	if(ll_is_active(rtl_cards))
	{
		struct llistnode *cur, *next;
		rtl8139dev_t *ent;
		ll_for_each_entry_safe(rtl_cards, cur, next, rtl8139dev_t *, ent)
		{
			//rtl8139_unload_device_pci(ent);
			ll_remove(rtl_cards, cur);
			//kfree(ent->rec_buf);
			//kfree(ent);
			ll_maybe_reset_loop(rtl_cards, cur, next);
		}
	}
	ll_destroy(rtl_cards);
	return 0;
}
