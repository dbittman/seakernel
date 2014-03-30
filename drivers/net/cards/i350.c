#include <kernel.h>
#include <sea/loader/module.h>
#include <modules/pci.h>
#include <modules/i350.h>
#include <sea/mm/pmap.h>
#include <sea/net/net.h>
#include <sea/mutex.h>
#include <sea/tm/process.h>
#include <sea/cpu/atomic.h>
#include <sea/cpu/interrupt.h>

int i350_int;
struct pmap i350_pmap;
struct i350_device *i350_dev;
struct net_dev *i350_net_dev;

int i350_receive_packet(struct net_dev *nd, struct net_packet *, int count);
int i350_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count);

int i350_get_mac(struct net_dev *nd, uint8_t mac[6])
{
	mac[0] = 1;
	mac[1] = 2;
	mac[2] = 3;
	mac[3] = 5;
	mac[4] = 4;
	mac[5] = 6;
	return 0;
}

struct net_dev_calls i350_net_callbacks = {
	i350_receive_packet,
	i350_transmit_packet,
	i350_get_mac,
	0,0,0
};

struct pci_device *get_i350_pci()
{
	struct pci_device *i350;
	i350 = pci_locate_device(0x8086, 0x1521);
	if(!i350)
		return 0;
	i350->flags |= PCI_ENGAGED;
	i350->flags |= PCI_DRIVEN;
	printk(KERN_DEBUG, "[i350]: found i350 device, initializing...\n");
	if(!(i350->pcs->command & 4))
		printk(KERN_DEBUG, "[ahci]: setting PCI command to bus mastering mode\n");
	unsigned short cmd = i350->pcs->command | 4;
	i350->pcs->command = cmd;
	pci_write_dword(i350->bus, i350->dev, i350->func, 4, cmd);
	i350_int = i350->pcs->interrupt_line+32;
	return i350;
}

void i350_write32(struct i350_device *dev, uint32_t reg, uint32_t value)
{
	addr_t addr = pmap_get_mapping(&i350_pmap, dev->mem + reg);
	volatile uint32_t *a = (uint32_t *)(addr);
	*a = value;
}

uint32_t i350_read32(struct i350_device *dev, uint32_t reg)
{
	addr_t addr = pmap_get_mapping(&i350_pmap, dev->mem + reg);
	volatile uint32_t *a = (uint32_t *)(addr);
	return *a;
}

void i350_reset(struct i350_device *dev)
{
	while((i350_read32(dev, E1000_CTRL) & E1000_CTRL_RESET))
		asm("pause");
	while(!(i350_read32(dev, E1000_STATUS) & E1000_STATUS_RESET_DONE))
		asm("pause");
	/* disable master */
	uint32_t tmp;
	tmp = i350_read32(dev, E1000_CTRL);
	tmp |= E1000_CTRL_GIO_MASTER_DISABLE;
	i350_write32(dev, E1000_CTRL, tmp);
	
	/* wait for master to disable */
	while((i350_read32(dev, E1000_STATUS) & E1000_STATUS_GIO_MASTER_ENABLE))
		asm("pause");
	
	/* issue port reset */
	tmp |= E1000_CTRL_RESET;
	i350_write32(dev, E1000_CTRL, tmp);
	
	/* wait for reset */
	while((i350_read32(dev, E1000_CTRL) & E1000_CTRL_RESET))
		asm("pause");
	while(!(i350_read32(dev, E1000_STATUS) & E1000_STATUS_RESET_DONE))
		asm("pause");
	
	/* re-enable master */
	tmp = i350_read32(dev, E1000_CTRL);
	tmp &= ~E1000_CTRL_GIO_MASTER_DISABLE;
	i350_write32(dev, E1000_CTRL, tmp);
}

void i350_allocate_receive_buffers(struct i350_device *dev)
{
	dev->rx_buffer_len = 0x1000;
	
	struct i350_receive_descriptor *r;
	addr_t rxring;
	r = kmalloc_ap(0x1000, &rxring);
	dev->receive_list_physical = rxring;
	dev->rx_list_count = (0x1000 / sizeof(struct i350_receive_descriptor));
	dev->receive_ring = r;
	kprintf("[i350]: allocated rx_list: %d descs\n", dev->rx_list_count);
	
	dev->rx_ring_virt_buffers = kmalloc(sizeof(addr_t) * dev->rx_list_count);
	
	for(unsigned int i=0;i<dev->rx_list_count;i++)
	{
		addr_t ba;
		dev->rx_ring_virt_buffers[i] = (addr_t)kmalloc_ap(0x1000, &ba);
		memset(r, 0, sizeof(*r));
		r->buffer = ba;
		r++;
	}
	
	uint32_t tmp = i350_read32(dev, E1000_RXDCTL);
	tmp &= ~(1 << 25);
	i350_write32(dev, E1000_RXDCTL, tmp);
	
	tmp = dev->rx_list_count / 8;
	i350_write32(dev, E1000_RDLEN0, tmp << 7);
	
	i350_write32(dev, E1000_SRRCTL0, 4);
	
	
	i350_write32(dev, E1000_RDBAL0, rxring & 0xFFFFFFFF);
	i350_write32(dev, E1000_RDBAH0, UPPER32(rxring));
	
	tmp |= (1<<25);
	i350_write32(dev, E1000_RXDCTL, tmp);
	
	while(!(i350_read32(dev, E1000_RXDCTL) & (1<<25))) asm("pause");
	
	tmp = i350_read32(dev, E1000_RCTL);
	i350_write32(dev, E1000_RCTL, tmp | (1<<1) | (1<<3) | (1<<4) | (1<<15));
	
}

void i350_allocate_transmit_buffers(struct i350_device *dev)
{
	dev->tx_buffer_len = 0x1000;
	
	struct i350_transmit_descriptor *r;
	addr_t txring;
	r = kmalloc_ap(0x1000, &txring);
	dev->transmit_list_physical = txring;
	dev->tx_list_count = (0x1000 / sizeof(struct i350_transmit_descriptor));
	dev->transmit_ring = r;
	kprintf("[i350]: allocated tx_list: %d descs\n", dev->tx_list_count);
	
	dev->tx_ring_virt_buffers = kmalloc(sizeof(addr_t) * dev->rx_list_count);
	
	for(unsigned int i=0;i<dev->tx_list_count;i++)
	{
		addr_t ba;
		dev->tx_ring_virt_buffers[i] = (addr_t)kmalloc_ap(0x1000, &ba);
		memset(r, 0, sizeof(*r));
		r->buffer = ba;
		r++;
	}
	
	uint32_t tmp = i350_read32(dev, E1000_RXDCTL);
	tmp &= ~(1 << 25);
	i350_write32(dev, E1000_TXDCTL, tmp);
	
	tmp = dev->tx_list_count / 8;
	i350_write32(dev, E1000_TDLEN0, tmp << 7);
	
	i350_write32(dev, E1000_TDBAL0, txring & 0xFFFFFFFF);
	i350_write32(dev, E1000_TDBAH0, UPPER32(txring));
	
	tmp |= (1<<25);
	i350_write32(dev, E1000_TXDCTL, tmp);
	
	while(!(i350_read32(dev, E1000_TXDCTL) & (1<<25))) asm("pause");
	
	tmp = i350_read32(dev, E1000_TCTL);
	i350_write32(dev, E1000_TCTL, tmp | (1<<1) | (1<<3));
	
}

void i350_init(struct i350_device *dev)
{
	dev->mem = dev->pci->pcs->bar0;
	kprintf("[i350]: using interrupt %d\n", i350_int);
	
	/* disable interrupts */
	i350_write32(dev, E1000_IMC, ~0);
	
	printk(KERN_DEBUG, "[i350]: resetting device\n");
	i350_reset(dev);
	
	/* disable interrupts again */
	i350_write32(dev, E1000_IMC, ~0);
	
	/* clear ILOS bit */
	uint32_t tmp, tmp2, tmp3, tmp4;
	tmp = i350_read32(dev, E1000_CTRL);
	tmp &= ~E1000_CTRL_ILOS;
	i350_write32(dev, E1000_CTRL, tmp);
	
	/* set Auto-Negotiation */
	tmp = i350_read32(dev, E1000_PCS_LCTL);
	tmp |= E1000_PCS_LCTL_AN_ENABLE;
	i350_write32(dev, E1000_PCS_LCTL, tmp);
	
	tm_delay_sleep(10);
	
	i350_allocate_receive_buffers(dev);
	i350_allocate_transmit_buffers(dev);
	
	tmp = i350_read32(dev, E1000_CTRL);
	tmp |= E1000_CTRL_SLU;
	tmp &= ~(E1000_CTRL_RXFC);
	i350_write32(dev, E1000_CTRL, tmp);
	
	i350_write32(dev, E1000_IMS, ~0);
	i350_write32(dev, E1000_RDT0, dev->rx_list_count-1);
	for(;;)
	{
		//tmp2 = i350_read32(dev, E1000_GPTC);
		//kprintf("**%d**\n", tmp2);
		tm_delay_sleep(2000);
	}

}

void i350_notify_packet_available()
{
	/* tell networking subsystem that this device has a packet available
	 * for reading. This doesn't do anything about the rx rings, that is
	 * up to the networking system to call i350_receive_packet */
	net_notify_packet_ready(i350_net_dev);
}

/* this is called by the networking subsystem, to read in packets */
int i350_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	if(count > 1) panic(0, "count > 1: NI");
	int num=1;
	struct i350_device *dev = i350_dev;
	mutex_acquire(dev->rx_queue_lock[0]);
	uint32_t head = i350_read32(dev, E1000_RDH0);
	uint32_t tail = i350_read32(dev, E1000_RDT0);
	
	uint32_t next = tail + 1;
	if(next > (dev->rx_list_count-1))
		next = 0;
	
	if(next == head) return 0;

	struct i350_receive_descriptor *r = &dev->receive_ring[next];
	
	kprintf("PACKET (%d %d %d): %x, %d\n", head, tail, next, r->status, r->length);
	sub_atomic(&nd->rx_pending, 1);
	
	memcpy(packets[0].data, (void *)(dev->rx_ring_virt_buffers[next]), r->length);
	packets[0].length = r->length;
	packets[0].flags = 0;
	
	i350_write32(dev, E1000_RDT0, next);
	mutex_release(dev->rx_queue_lock[0]);
	return num;
}

int i350_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	if(count > 1) panic(0, "count > 1: NI");
	struct i350_device *dev = i350_dev;
	mutex_acquire(dev->tx_queue_lock[0]);
	uint32_t head = i350_read32(dev, E1000_TDH0);
	uint32_t tail = i350_read32(dev, E1000_TDT0);
	
	memcpy((void *)(dev->tx_ring_virt_buffers[tail]), packets[0].data, packets[0].length);
	dev->transmit_ring[tail].length = packets[0].length;
	dev->transmit_ring[tail].cmd = (1 | (1<<3) | (1<<1));
	int old = tail;
	kprintf("SEND: %d\n", tail);
	tail++;
	if(tail == dev->tx_list_count)
		tail=0;
	
	i350_write32(dev, E1000_TDT0, tail);
	mutex_release(dev->tx_queue_lock[0]);
	return count;
}

void i350_link_status_change()
{
	kprintf("[i350]: link status changed\n");
}

void i350_transmitted_packet()
{
	//kprintf("[i350]: transmit complete\n");
}

void i350_rx_miss()
{
	kprintf("[i350]: warning - missed packet\n");
}

void i350_error_interrupt()
{
	kprintf("[i350]: error - fatal error interrupt received\n");
}

void i350_interrupt()
{
	uint32_t t = i350_read32(i350_dev, E1000_ICR);
	if(!(t & (1 << 31)))
		return;
	
	if((t & E1000_ICR_LSC))
		i350_link_status_change();
	if((t & E1000_ICR_RXDW))
		i350_notify_packet_available();
	if((t & E1000_ICR_TXDW))
		i350_transmitted_packet();
	if((t & E1000_ICR_RXMISS))
		i350_rx_miss();
	if((t & E1000_ICR_FER) || (t & E1000_ICR_PCIEX))
		i350_error_interrupt();
	
	/* clear interrupt cause */
	i350_write32(i350_dev, E1000_ICR, t);
}

void i350_interrupt_lvl2()
{
	if(i350_net_dev->rx_pending)
	{
		struct net_packet packet[1];
		i350_receive_packet(i350_net_dev, packet, 1);
		net_receive_packet(i350_net_dev, packet, 1);
	}
}

int irq1;

int module_install()
{
	struct pci_device *i350;
	i350 = get_i350_pci();
	if(!i350) {
		printk(KERN_DEBUG, "[i350]: no such device found!\n");
		return -ENOENT;
	}
	pmap_create(&i350_pmap, 0);
	struct i350_device *dev = kmalloc(sizeof(struct i350_device));
	dev->pci = i350;
	irq1=arch_interrupt_register_handler(i350_int, i350_interrupt, i350_interrupt_lvl2);
	i350_dev = dev;
	dev->tx_queue_lock[0] = mutex_create(0, 0);
	dev->rx_queue_lock[0] = mutex_create(0, 0);
	i350_net_dev = net_add_device(&i350_net_callbacks, 0);
	i350_init(dev);
	
	return 0;
}

int module_tm_exit()
{
	return 0;
}
