#include <kernel.h>
#include <module.h>
#include <pmap.h>
#include <mutex.h>
#include <memory.h>
#include <types.h>
#include <modules/ahci.h>
#include <modules/pci.h>
#include <asm/system.h>
#include <isr.h>
#include <block.h>
#include <symbol.h>
struct pci_device *ahci_pci;
int ahci_int = 0;
struct hba_memory *hba_mem;
struct pmap *ahci_pmap;
struct ahci_device *ports[32];
int ahci_major=0;
struct pci_device *get_ahci_pci()
{
	struct pci_device *ahci = pci_locate_class(0x1, 0x6);
	if(!ahci)
		return 0;
	ahci->flags |= PCI_ENGAGED;
	ahci->flags |= PCI_DRIVEN;
	hba_mem = (void *)(addr_t)ahci->pcs->bar5;
	if(!(ahci->pcs->command & 4))
		printk(0, "[ahci]: setting PCI command to bus mastering mode\n");
	unsigned short cmd = ahci->pcs->command | 4;
	ahci->pcs->command = cmd;
	pci_write_dword(ahci->bus, ahci->dev, ahci->func, 4, cmd);
	/* of course, we need to map a virtual address to physical address
	 * for paging to not hate on us... */
	hba_mem = (void *)pmap_get_mapping(ahci_pmap, (addr_t)hba_mem);
	printk(0, "[ahci]: mapping hba_mem to %x -> %x\n", hba_mem, ahci->pcs->bar5);
	printk(0, "[ahci]: using interrupt %d\n", ahci->pcs->interrupt_line+32);
	ahci_int = ahci->pcs->interrupt_line+32;
	return ahci;
}

int read_partitions(struct ahci_device *dev, char *node, int port)
{
	addr_t p = find_kernel_function("enumerate_partitions");
	if(!p)
		return 0;
	int d = GETDEV(ahci_major, port);
	int (*e_p)(int, int, struct partition *);
	e_p = (int (*)(int, int, struct partition *))p;
	struct partition part;
	int i=0;
	while(i<64)
	{
		/* Returns the i'th partition of device 'd' into info struct part. */
		int r = e_p(i, d, &part);
		if(!r)
			break;
		if(part.sysid)
		{
			printk(0, "[ahci]: %d: read partition start=%d, len=%d\n", port, part.start_lba, part.length);
			int a = port;
			char tmp[17];
			memset(tmp, 0, 17);
			sprintf(tmp, "%s%d", node, i+1);
			devfs_add(devfs_root, tmp, S_IFBLK, ahci_major, a+(i+1)*32);
		}
		memcpy(&(dev->part[i]), &part, sizeof(struct partition));
		i++;
	}
	
	return 0;
}

void ahci_create_device(struct ahci_device *dev)
{
	char node[16];
	char c = 'a';
	if(dev->idx > 25) c = 'A';
	sprintf(node, "sd%c", (dev->idx % 26) + c);
	dev->node = devfs_add(devfs_root, node, S_IFBLK, ahci_major, dev->idx);
	read_partitions(dev, node, dev->idx);
}


void ahci_interrupt_handler()
{
	int i;
	for(i=0;i<32;i++) {
		if(hba_mem->interrupt_status & (1 << i)) {
			hba_mem->ports[i].interrupt_status = ~0;
			hba_mem->interrupt_status = (1 << i);
			ahci_flush_commands((struct hba_port *)&hba_mem->ports[i]);
		}
	}
}

/* since a DMA transfer must write to contiguous physical RAM, we need to allocate
 * buffers that allow us to create PRDT entries that do not cross a page boundary.
 * That means that each PRDT entry can transfer a maximum of PAGE_SIZE bytes (for
 * 0x1000 page size, that's 8 sectors). Thus, we allocate a buffer that is page aligned, 
 * in a multiple of PAGE_SIZE, so that the PRDT will write to contiguous physical ram
 * (the key here is that the buffer need not be contiguous across multiple PRDT entries).
 */
int ahci_rw_multiple_do(int rw, int min, u64 blk, char *out_buffer, int count)
{
	uint32_t length = count * ATA_SECTOR_SIZE;
	
	int d = min % 32;
	int p = min / 32;
	struct ahci_device *dev = ports[d];
	uint32_t part_off=0, part_len=0;
	u64 end_blk = dev->identify.lba48_addressable_sectors;
	if(p > 0) {
		part_off = dev->part[p-1].start_lba;
		part_len = dev->part[p-1].length;
		end_blk = part_len + part_off;
	}
	blk += part_off;
	
	if(blk >= end_blk)
		return 0;
	
	if((blk+count) > end_blk)
		count = end_blk - blk;
	
	if(!count)
		return 0;
	
	int num_pages = ((ATA_SECTOR_SIZE * (count-1)) / PAGE_SIZE) + 1;
	assert(length <= (unsigned)num_pages * 0x1000);
	unsigned char *buf = kmalloc_a(0x1000 * num_pages);
	
	int num_read_blocks = count;
	
	if(rw == WRITE)
		memcpy(buf, out_buffer, length);
	
	mutex_acquire(&dev->lock);
	
	struct hba_port *port = (struct hba_port *)&hba_mem->ports[dev->idx];
	int slot=0;
	if(!ahci_port_dma_data_transfer(hba_mem, port, dev, slot, rw == WRITE ? 1 : 0, (addr_t)buf, count, blk))
		num_read_blocks = 0;
	
	mutex_release(&dev->lock);
	
	if(rw == READ && num_read_blocks)
		memcpy(out_buffer, buf, length);
	
	kfree(buf);
	return num_read_blocks * ATA_SECTOR_SIZE;
}

/* and then since there is a maximum transfer amount because of the page size
 * limit, wrap the transfer function to allow for bigger transfers than that even.
 */
int ahci_rw_multiple(int rw, int min, u64 blk, char *out_buffer, int count)
{
	int i=0;
	int ret=0;
	int c = count;
	for(i=0;i<count;i+=(PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE)
	{
		int n = (PRDT_MAX_ENTRIES * PRDT_MAX_COUNT) / ATA_SECTOR_SIZE;
		if(n > c)
			n=c;
		ret += ahci_rw_multiple_do(rw, min, blk+i, out_buffer + ret, n);
		c -= n;
	}
	return ret;
}

int ahci_rw_single(int rw, int dev, u64 blk, char *buf)
{
	return ahci_rw_multiple_do(rw, dev, blk, buf, 1);
}

int ioctl_ahci(int min, int cmd, long arg)
{
	return -EINVAL;
}

int irq1;
int module_install()
{
	printk(0, "[ahci]: initializing ahci driver...\n");
	ahci_pmap = pmap_create(0, 0);
	if(!(ahci_pci = get_ahci_pci()))
	{
		printk(0, "[ahci]: no AHCI controllers present!\n");
		pmap_destroy(ahci_pmap);
		return -ENOENT;
	}
	irq1 = register_interrupt_handler(ahci_int, ahci_interrupt_handler, 0);
	ahci_major = set_availablebd(ahci_rw_single, ATA_SECTOR_SIZE, ioctl_ahci, ahci_rw_multiple, 0);
	ahci_init_hba(hba_mem);
	ahci_probe_ports(hba_mem);
	return 0;
}

int module_deps(char *b)
{
	write_deps(b, "pci,:");
	return KVERSION;
}

int module_exit()
{
	int i;
	unregister_block_device(ahci_major);
	unregister_interrupt_handler(ahci_int, irq1);
	for(i=0;i<32;i++)
	{
		if(ports[i]) {
			mutex_destroy(&(ports[i]->lock));
			kfree(ports[i]->clb_virt);
			kfree(ports[i]->fis_virt);
			for(int j=0;j<HBA_COMMAND_HEADER_NUM;j++)
				kfree(ports[i]->ch[j]);
			kfree(ports[i]);
		}
	}
	pmap_destroy(ahci_pmap);
	ahci_pci->flags = 0;
	return 0;
}
