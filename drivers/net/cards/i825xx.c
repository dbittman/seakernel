#include <sea/kernel.h>
#include <sea/dm/dev.h>
#include <modules/pci.h>
#include <sea/loader/symbol.h>
#include <sea/dm/char.h>
#include <sea/loader/module.h>
#define NUM_RX_DESCRIPTORS	768
#define NUM_TX_DESCRIPTORS	768

#define CTRL_FD				(1 << 0)
#define CTRL_ASDE			(1 << 5)
#define CTRL_SLU			(1 << 6)
int i8_maj=-1, i8_min=0;
// RX and TX descriptor structures
typedef struct __attribute__((packed)) i825xx_rx_desc_s 
{
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint16_t	checksum;
	volatile uint8_t	status;
	volatile uint8_t	errors;
	volatile uint16_t	special;
} i825xx_rx_desc_t;

typedef struct __attribute__((packed)) i825xx_tx_desc_s 
{
	volatile uint64_t	address;
	
	volatile uint16_t	length;
	volatile uint8_t	cso;
	volatile uint8_t	cmd;
	volatile uint8_t	sta;
	volatile uint8_t	css;
	volatile uint16_t	special;
} i825xx_tx_desc_t;

typedef struct i825xx_dev_s
{
	unsigned addr, inter;
	struct pci_device *device;
	volatile uint8_t		*rx_desc_base;
	volatile i825xx_rx_desc_t	*rx_desc[NUM_RX_DESCRIPTORS];	// receive descriptor buffer
	volatile uint16_t	rx_tail;
	
	volatile uint8_t		*tx_desc_base;
	volatile i825xx_tx_desc_t	*tx_desc[NUM_TX_DESCRIPTORS];	// transmit descriptor buffer
	volatile uint16_t	tx_tail;
	
	struct inode *node;
	
	struct i825xx_dev_s *next, *prev;
} i825xxdev_t;

volatile i825xxdev_t *cards;

i825xxdev_t *create_new_device(unsigned addr, struct pci_device *device)
{
	i825xxdev_t *d = (i825xxdev_t *)kmalloc(sizeof(i825xxdev_t));
	d->addr = addr;
	d->device = device;
	i825xxdev_t *t = (i825xxdev_t *)cards;
	cards = d;
	d->next = t;
	if(t)
		t->prev=d;
	
	return d;
}

void delete_device(i825xxdev_t *d)
{
	if(d->prev)
		d->prev->next = d->next;
	if(d->next)
		d->next->prev = d->prev;
	if(d == cards)
		cards = d->next;
	kfree(d);
}

int i825xx_init(i825xxdev_t *dev)
{
	return 0;
}

int i825xx_int(registers_t *regs)
{
	return 0;
}

int i825xx_load_device_pci(struct pci_device *device)
{
	int addr;
	char ret=0;
	if(!(addr = pci_get_base_address(device)))
	{
		device->flags |= PCI_ERROR;
		return 1;
	}
	i825xxdev_t *dev = create_new_device(addr, device);
	printk(1, "[i825xx]: Initiating i825xx controller (%x.%x.%x)...\n", 
		device->bus, device->dev, device->func);
	if(i825xx_init(dev))
		ret++;
	
	if(ret){
		printk(1, "[i825xx]: Device error when trying to initialize\n");
		device->flags |= PCI_ERROR;
		return -1;
	}
	struct inode *i = dfs_cn("i825xx", S_IFCHR, i8_maj, i8_min++);
	dev->node = i;
	printk(1, "[i825xx]: Success!\n");
	device->flags |= PCI_ENGAGED;
	device->flags |= PCI_DRIVEN;
	dev->inter = device->pcs->interrupt_line;
	interrupt_register_handler(dev->inter, (isr_t)&i825xx_int);
	return 0;
}

int i825xx_unload_device_pci(i825xxdev_t *dev)
{
	if(!dev) return 0;
	struct pci_device *device = dev->device;
	printk(1, "[i825xx]: Unloading device (%x.%x.%x)\n", 
		device->bus, device->dev, device->func);
	device->flags &= ~PCI_ENGAGED;
	device->flags &= ~PCI_DRIVEN;
	iremove_force(dev->node);
	interrupt_unregister_handler(dev->inter, (isr_t)&i825xx_int);
	delete_device(dev);
	return 0;
}

int i825xx_rw_main(int rw, int min, char *buf, int count)
{
	return 0;
}

int ioctl_i825xx(int min, int cmd, int arg)
{
	return 0;
}

int module_install()
{
	i8_min=0;
	cards=0;
	i8_maj = dm_set_available_char_device(i825xx_rw_main, ioctl_i825xx, 0);
	int i=0;
	printk(1, "[i825xx]: Scanning PCI bus...\n");
	while(1) {
		struct pci_device *dev = pci_locate_devices(0x8086, 0x1229, i);
		if(!dev)
			break;
		i825xx_load_device_pci(dev);
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
	printk(1, "[i825xx]: Shutting down all cards...\n");
	while(cards) /* this call updates 'cards' within it. */
		i825xx_unload_device_pci((i825xxdev_t *)cards);
	unregister_char_device(i8_maj);
	return 0;
}
