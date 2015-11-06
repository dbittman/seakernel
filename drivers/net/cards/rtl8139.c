#include <sea/string.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <sea/loader/symbol.h>
#include <sea/dm/char.h>
#include <sea/loader/module.h>
#include <sea/fs/devfs.h>
#include <sea/mm/dma.h>
#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/vsprintf.h>
#include <sea/cpu/cpu-io.h>
#include <sea/mm/kmalloc.h>
#include <sea/cpu/interrupt.h>
#include <sea/mm/dma.h>
#include <sea/mutex.h>
#include <sea/cpu/processor.h>
#include <sea/errno.h>

#include <sea/types.h>
#include <sea/tm/timing.h>
int rtl8139_maj=-1;
typedef struct rtl8139_dev_s
{
	unsigned addr, tx_o, rx_o;
	addr_t rec_buf, rec_buf_virt;
	int inter_id, inter;
	struct pci_device *device;
	struct dma_region rx_reg, tx_buffer[4];
	struct mutex tx_lock;
	int tx_num;
	unsigned short hwaddr[3];
	struct net_dev *net_dev;
} rtl8139dev_t;

rtl8139dev_t *devs[16];

#define RX_BUF_SIZE (64 * 1024)
int rtl8139_receive_packet(struct net_dev *nd, struct net_packet *, int count);
int rtl8139_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count);
int rtl8139_set_flags(struct net_dev *nd, int flags);
int rtl8139_get_mac(struct net_dev *nd, uint8_t mac[6])
{
	rtl8139dev_t *rtldev = nd->data;
	mac[0] = rtldev->hwaddr[0] & 0xFF;
	mac[1] = rtldev->hwaddr[0] >> 8;
	mac[2] = rtldev->hwaddr[1] & 0xFF;
	mac[3] = rtldev->hwaddr[1] >> 8;
	mac[4] = rtldev->hwaddr[2] & 0xFF;
	mac[5] = rtldev->hwaddr[2] >> 8;
	return 0;
}

struct net_dev_calls rtl8139_net_callbacks = {
	rtl8139_receive_packet,
	rtl8139_transmit_packet,
	rtl8139_get_mac,
	rtl8139_set_flags,
	0
};


rtl8139dev_t *create_new_device(unsigned addr, struct pci_device *device)
{
	rtl8139dev_t *d = (rtl8139dev_t *)kmalloc(sizeof(rtl8139dev_t));
	d->addr = addr;
	d->device = device;
	return d;
}
#define EE_SHIFT_CLK    0x04    //!< EEPROM shift clock.
#define EE_CS           0x08    //!< EEPROM chip select.
#define EE_DATA_WRITE   0x02    //!< EEPROM chip data in.
#define EE_WRITE_0      0x00    //!< EEPROM write 0.
#define EE_WRITE_1      0x02    //!< EEPROM write 1.
#define EE_DATA_READ    0x01    //!< EEPROM chip data out.
#define EE_ENB          (0x80 | EE_CS)
#define EE_READ_CMD   (6 << 6)
#define delay_eeprom() inl(eeaddr)
uint32_t rtl8139_read_eeprom(rtl8139dev_t *dev, int loc)
{
	int i;
	uint32_t ret=0;
	long eeaddr = dev->addr + 0x50;
	int cmd = loc | EE_READ_CMD;

	/* don't ask me what the fuck is going on here */
	outb(eeaddr, EE_ENB & ~EE_CS);
	outb(eeaddr, EE_ENB);

	for(i=10;i>=0;--i) {
		int data = (cmd & (1 << i)) ? EE_DATA_WRITE : 0;
		outb(eeaddr, EE_ENB | data);
		delay_eeprom();
		outb(eeaddr, EE_ENB | data | EE_SHIFT_CLK);
		delay_eeprom();
	}
	outb(eeaddr, EE_ENB);
	delay_eeprom();

	for(i=16;i>0;--i) {
		outb(eeaddr, EE_ENB | EE_SHIFT_CLK);
		delay_eeprom();
		ret = (ret << 1) | ((inb(eeaddr) & EE_DATA_READ) ? 1 : 0);
		outb(eeaddr, EE_ENB);
		delay_eeprom();
	}

	outb(eeaddr, ~EE_CS);
	return ret;
}

int rtl8139_reset(int base_addr)
{
	unsigned timeout=10;
	outb(base_addr+0x52, 0x00);
	char done=0;
	while(1) {
		//outb(base_addr+0x50, inb(base_addr+0x50)|(0x03 << 6));

		tm_thread_delay(ONE_MILLISECOND * 10);
		if(done)
			break;
		outb(base_addr+0x37, 0x10);
		while(--timeout){
		tm_thread_delay(ONE_MILLISECOND * 10);
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
		tm_thread_delay(ONE_MILLISECOND * 10);
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
	dev->rx_reg.p.size = RX_BUF_SIZE;
	dev->rx_reg.p.alignment = 0x1000;
	int ret = mm_allocate_dma_buffer(&dev->rx_reg);
	if(ret == -1) {
		printk(0, "[rtl8139]: failed to allocate dma buffer\n");
		return -1;
	}
	dev->rec_buf = dev->rx_reg.p.address;
	dev->rec_buf_virt = dev->rx_reg.v;

	for(int i=0;i<4;i++) {
		dev->tx_buffer[i].p.size = 0x1000;
		dev->tx_buffer[i].p.alignment = 0x1000;
		if(mm_allocate_dma_buffer(&dev->tx_buffer[i]) == -1)
			return -1;
	}
	dev->tx_num = 0;

	if(rtl8139_read_eeprom(dev, 0) != 0xFFFF) {
		for(int i=0;i<3;i++) {
			dev->hwaddr[i] = rtl8139_read_eeprom(dev, i + 7);
		}
	}

	outb(dev->addr+0x50, 0xC0);

	// get the card out of low power mode
	outb(dev->addr+0x52, 0);

	// write the RxBuffer's address
	outl(dev->addr+0x30, (addr_t)dev->rec_buf);

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
	//outb(dev->addr+0x37, 0x08 | 0x04);

	// enable all good irqs
	outw(dev->addr+0x3C, 15);
	outw(dev->addr+0x3E, 0xffff);
	return 0;
}

int rtl8139_set_flags(struct net_dev *nd, int flags)
{
	rtl8139dev_t *dev = nd->data;
	if(!(flags & IFACE_FLAG_UP) && (nd->flags & IFACE_FLAG_UP)) {
		/* set interface down */
		outb(dev->addr+0x37, 0);
	} else if((flags & IFACE_FLAG_UP) && !(nd->flags & IFACE_FLAG_UP)) {
		/* set interface up */
		outb(dev->addr+0x37, 0x08 | 0x04);
	}
	return flags;
}

int rtl8139_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	/* TODO: count */
	rtl8139dev_t *dev = nd->data;
	if(packets[0].length > 0x1000)
		return -EIO;
	mutex_acquire(&dev->tx_lock);

	/* wait for the thing */
	while(1) {
		uint32_t status = inl(dev->addr + 0x10 + dev->tx_num * 4);
		if((status & (1 << 13)))
			break;
		cpu_pause();
	}
	printk(0, "memcpy %d: %x <- %x, for %d\n", dev->tx_num, dev->tx_buffer[dev->tx_num].v, packets[0].data, packets[0].length);
	memcpy((void *)dev->tx_buffer[dev->tx_num].v, packets[0].data, packets[0].length);
	if(packets[0].length < 0x1000)
		memset((void *)(dev->tx_buffer[dev->tx_num].v + packets[0].length), 0, 0x1000 - packets[0].length);

	outl(dev->addr + 0x20 + dev->tx_num * 4, dev->tx_buffer[dev->tx_num].p.address);
	outl(dev->addr + 0x10 + dev->tx_num * 4, 0x3F0000 | (packets[0].length & 0x1FFF));

	dev->tx_num++;
	dev->tx_num %= 4;

	mutex_release(&dev->tx_lock);
	return 1;
}
int rtl8139_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	rtl8139dev_t *dev = nd->data;
	uint8_t *buffer;
	uint16_t length, info;
	int num=0;
	while(1)
	{
		uint32_t cmd = inb(dev->addr + 0x37);
		if(cmd & 1) {
			return 0;
		}

		buffer = (void *)(dev->rec_buf_virt + dev->rx_o);
		info = *(uint16_t *)buffer;
		buffer += 2;

			length = *(uint16_t *)buffer;
			//printk(0, " %d got packet len %d, invalid? %d\n", dev->rx_o, length, info & 1);
			buffer += 2;
			dev->rx_o += 4;
			if(length >= 14 && (info & 1)) /* larger than ethernet frame header */
			{
				uint8_t *data = packets[0].data;
				packets[0].length = length - 4;

				if((dev->rx_o + length - 4) >= RX_BUF_SIZE) {
					memcpy(data, buffer, RX_BUF_SIZE - dev->rx_o);
					memcpy(data + (RX_BUF_SIZE - dev->rx_o),
							(void *)dev->rec_buf_virt, (length - 4) - (RX_BUF_SIZE - dev->rx_o));
				} else {
					memcpy(data, buffer, length - 4);
				}

				num = 1;

			}

			dev->rx_o += length;
		dev->rx_o = (dev->rx_o + 3) & ~3;
		dev->rx_o %= RX_BUF_SIZE;

		outw(dev->addr + 0x38, dev->rx_o - 0x10);

		/* TODO */
		break;
	}


	return num;
}

void do_recieve(rtl8139dev_t *dev, unsigned short data)
{
	net_notify_packet_ready(dev->net_dev);
}

void rtl8139_int_1(struct registers *regs, int int_no, int flags)
{
	for(int i = 0;i<16;i++) {
		rtl8139dev_t *t=devs[i];
		if(!t)
			continue;
		if((unsigned)(t->inter+IRQ0) == regs->int_no)
		{
			unsigned short data = inw(t->addr + 0x3E);
			if(data & 0x01)
				do_recieve(t, data);
			if(data & (1 << 2)) {
				/* need to read all status registers */
				for(int j=0;j<4;j++)
					inw(t->addr + 0x10 + j * 4);
			}
			outw(t->addr + 0x3E, data);
		}
	}
	return;
}

rtl8139dev_t *rtl8139_load_device_pci(struct pci_device *device)
{
	int addr;
	char ret=0;
	if(!(addr = pci_get_base_address(device)))
	{
		device->flags |= PCI_ERROR;
		return 0;
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
		return 0;
	}

	sys_mknod("/dev/rtl8139", S_IFCHR | 0600, GETDEV(rtl8139_maj, 0));
	device->flags |= PCI_ENGAGED;
	device->flags |= PCI_DRIVEN;
	dev->inter = device->pcs->interrupt_line;
	dev->inter_id = cpu_interrupt_register_handler(dev->inter + IRQ0, rtl8139_int_1);
	mutex_create(&dev->tx_lock, 0);
	printk(1, "[rtl8139]: registered interrupt line %d\n", dev->inter);
	printk(1, "[rtl8139]: Success!\n");
	return dev;
}

int rtl8139_unload_device_pci(rtl8139dev_t *dev)
{
	if(!dev) return 0;
	struct pci_device *device = dev->device;
	printk(1, "[rtl8139]: Unloading device (%x.%x.%x)\n", device->bus, 
			device->dev, device->func);
	device->flags &= ~PCI_ENGAGED;
	device->flags &= ~PCI_DRIVEN;
	mutex_destroy(&dev->tx_lock);
	/* TODO */
	//devfs_remove(dev->node);
	cpu_interrupt_unregister_handler(dev->inter, dev->inter_id);
	return 0;
}

int rtl8139_rw_main(int rw, int min, char *buf, size_t count)
{
	return 0;
}

int ioctl_rtl8139(int min, int cmd, long int arg)
{
	return 0;
}

int module_install(void)
{
	rtl8139_maj = dm_set_available_char_device(rtl8139_rw_main, ioctl_rtl8139, 0);
	int i=0;
	memset(devs, 0, sizeof(rtl8139dev_t *) * 16);
	printk(1, "[rtl8139]: Scanning PCI bus...\n");
	while(i < 1) {
		struct pci_device *dev = pci_locate_devices(0x10ec, 0x8139, i);
		if(dev) {
			rtl8139dev_t *rdev = rtl8139_load_device_pci(dev);
			if(!rdev) {
				i++;
				continue;
			}
			struct net_dev *rtl8139_net_dev = net_add_device(&rtl8139_net_callbacks, rdev);
			rtl8139_net_dev->data_header_len = 14;
			rtl8139_net_dev->hw_address_len = 6;
			rtl8139_net_dev->hw_type = NET_HWTYPE_ETHERNET;
			rtl8139_net_dev->brate = 10000000; /* TODO */
			rdev->net_dev = rtl8139_net_dev;
			rtl8139_net_dev->data = rdev;
			devs[i] = rdev;

			i++;
		}
		if(!dev)
			break;
	}
	return 0;
}

int module_exit(void)
{
	dm_unregister_char_device(rtl8139_maj);
	//rtl8139_unload_device_pci(rtldev);
	///* TODO */
	return 0;
}

