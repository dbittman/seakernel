#include <kernel.h>
#include <module.h>
#include <modules/pci.h>
#include <modules/i350.h>
#include <pmap.h>

int i350_int;
struct pmap i350_pmap;
struct i350_device *i350_dev;

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
	dev->rx_list_count = (0x1000 / sizeof(struct i350_receive_descriptor));
	dev->receive_ring = r;
	kprintf("[i350]: allocated rx_list: %d descs\n", dev->rx_list_count);

	for(int i=0;i<dev->rx_list_count;i++)
	{
		addr_t ba;
		kmalloc_ap(0x1000, &ba);
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
	i350_write32(dev, E1000_RDBAH0, (rxring >> 32) & 0xFFFFFFFF);
	
	

	tmp |= (1<<25);
	i350_write32(dev, E1000_RXDCTL, tmp);


	tmp = i350_read32(dev, E1000_RCTL);
	i350_write32(dev, E1000_RCTL, tmp | (1<<1) | (1<<3) | (1<<4) | (1<<15));

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
	uint32_t tmp, tmp2, tmp3;
	tmp = i350_read32(dev, E1000_CTRL);
	tmp &= ~E1000_CTRL_ILOS;
	i350_write32(dev, E1000_CTRL, tmp);
	
	/* set Auto-Negotiation */
	tmp = i350_read32(dev, E1000_PCS_LCTL);
	tmp |= E1000_PCS_LCTL_AN_ENABLE;
	i350_write32(dev, E1000_PCS_LCTL, tmp);
	
	delay_sleep(10);
	
	i350_allocate_receive_buffers(dev);
	
	tmp = i350_read32(dev, E1000_CTRL);
	tmp |= E1000_CTRL_SLU;
	tmp &= ~(E1000_CTRL_RXFC);
	i350_write32(dev, E1000_CTRL, tmp);
	
	i350_write32(dev, E1000_IMS, ~0);
	i350_write32(dev, E1000_RDT0, dev->rx_list_count-1);
	for(;;)
	{
		//tmp = i350_read32(dev, E1000_RDH0);
		//tmp2 = i350_read32(dev, E1000_RDT0);
		//tmp3 = i350_read32(dev, E1000_GPRC);
		//kprintf("%d %d: %x\n", tmp, tmp2, tmp3);
		//if(tmp) {
		//	struct i350_receive_descriptor *r;
		//	r = dev->receive_ring;
		//	kprintf("%x %x %x\n", r->status, r->error, r->length);
		//}
		//delay_sleep(400);
	}

}

void i350_notify_packet_available()
{
	/* tell networking subsystem that this device has a packet available
	 * for reading. This doesn't do anything about the rx rings, that is
	 * up to the networking system to call i350_receive_packet */
}

int i350_receive_packet()
{
	struct i350_device *dev = i350_dev;
	uint32_t head = i350_read32(dev, E1000_RDH0);
	uint32_t tail = i350_read32(dev, E1000_RDT0);
	
	uint32_t next = tail + 1;
	if(next > (dev->rx_list_count-1))
		next = 0;
	
	if(next == head) return 0;

	struct i350_receive_descriptor *r = &dev->receive_ring[next];
	
	kprintf("GOT PACKET (%d %d %d): %x, %d\n", head, tail, next, r->status, r->length);
	
	i350_write32(dev, E1000_RDT0, next);
	return 1;
}

void i350_link_status_change()
{
	kprintf("[i350]: link status changed\n");
}

void i350_transmitted_packet()
{
	
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
	irq1=register_interrupt_handler(i350_int, i350_interrupt, 0);
	i350_dev = dev;
	i350_init(dev);
	
	return 0;
}

int module_exit()
{
	return 0;
}
