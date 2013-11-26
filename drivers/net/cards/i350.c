#include <kernel.h>
#include <module.h>
#include <modules/pci.h>
#include <modules/i350.h>
#include <pmap.h>

int i350_int;
struct pmap i350_pmap;


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
	volatile uint32_t *a = (uint32_t *)(dev->mem + reg);
	*a = value;
}

uint32_t i350_read32(struct i350_device *dev, uint32_t reg)
{
	volatile uint32_t *a = (uint32_t *)(dev->mem + reg);
	return *a;
}

void i350_pcs_write32(struct i350_device *dev, uint32_t reg, uint32_t value)
{
	volatile uint32_t *a = (uint32_t *)(dev->pcsmem + (reg-0x4000));
	*a = value;
}

uint32_t i350_pcs_read32(struct i350_device *dev, uint32_t reg)
{
	volatile uint32_t *a = (uint32_t *)(dev->pcsmem + (reg-0x4000));
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
	
}

void i350_init(struct i350_device *dev)
{
	addr_t mem = pmap_get_mapping(&i350_pmap, dev->pci->pcs->bar0);
	dev->mem = mem;
	dev->pcsmem = pmap_get_mapping(&i350_pmap, dev->pci->pcs->bar0+0x4000);
	kprintf("[i350]: using interrupt %d\n", i350_int);
	kprintf("[i350]: mapping %x -> %x\n", mem, dev->pci->pcs->bar0);
	kprintf("[i350]: mapping %x -> %x\n", dev->pcsmem, dev->pci->pcs->bar0+0x4000);
	
	/* disable interrupts */
	i350_write32(dev, E1000_IMC, ~0);
	
	printk(KERN_DEBUG, "[i350]: resetting device\n");
	i350_reset(dev);
	
	/* disable interrupts again */
	i350_write32(dev, E1000_IMC, ~0);
	
	/* clear ILOS bit */
	uint32_t tmp;
	tmp = i350_read32(dev, E1000_CTRL);
	tmp &= ~E1000_CTRL_ILOS;
	i350_write32(dev, E1000_CTRL, tmp);
	
	/* set Auto-Negotiation */
	tmp = i350_pcs_read32(dev, E1000_PCS_LCTL);
	tmp |= E1000_PCS_LCTL_AN_ENABLE;
	i350_pcs_write32(dev, E1000_PCS_LCTL, tmp);
	
	delay_sleep(10);
	
	i350_allocate_receive_buffers(dev);
	/* write the registers for the receive ring */
	
}

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
	i350_init(dev);
	
	return 0;
}

int module_exit()
{
	return 0;
}
