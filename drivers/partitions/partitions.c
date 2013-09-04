#include <kernel.h>
#include <dev.h>
#include <fs.h>
#include <block.h>
#include <symbol.h>
struct partition {
	char flag;
	char ext;
	char i_dont_care[2];
	char sysid;
	char again_dont_care[3];
	unsigned int start_lba;
	unsigned int length;
}__attribute__((packed));

/* Okay. What the actual fuck. */
int parse_extended_partitions(int num, int dev, struct partition *ext, 
	struct partition *part, int prev_lba)
{
	unsigned char buf[512];
	printk(0, "[partition]: Reading extended partition at %d\n", 
		(ext->start_lba + prev_lba));
	do_block_rw(READ, dev, (ext->start_lba + prev_lba)*512, (char *)buf, 0);
	/* Only two entries */
	struct partition ptable[2];
	memcpy(ptable, buf+0x1BE, 32);
	int i;
	for(i=0;i<2;i++)
	{
		if(ptable[i].sysid == 0xF || ptable[i].sysid == 0x5)
		{
			/* Extended Partitions */
			prev_lba += ext->start_lba;
			num = parse_extended_partitions(num, dev, &ptable[i], part, prev_lba);
			if(num == -1)
				return -1;
			continue;
		}
		if(!num--) {
			ptable[i].start_lba += (ext->start_lba+prev_lba);
			memcpy(part, &ptable[i], sizeof(*part));
			part->ext=1;
			return num;
		}
	}
	return num;
}

int enumerate_partitions(int num, int dev, struct partition *part)
{
	unsigned char buf[512];
	do_block_rw(READ, dev, 0, (char *)buf, 0);
	struct partition ptable[4];
	memcpy(ptable, buf+0x1BE, 64);
	int i;
	for(i=0;i<4;i++)
	{
		if(ptable[i].sysid == 0xF || ptable[i].sysid == 0x5)
		{
			/* Extended Partitions */
			kprintf("UNTESTED: parsing extended partition in device=%x\n", dev);
			num = parse_extended_partitions(num, dev, &ptable[i], part, 0);
			if(num == -1)
				return 1;
			continue;
		}
		if(!num--) {
			memcpy(part, &ptable[i], sizeof(*part));
			part->ext=0;
			return 1;
		}
	}
	return 0;
}

int module_install()
{
	add_kernel_symbol(enumerate_partitions);
	printk(1, "[partitions]: Telling any HD drivers to reload their partition information\n");
	block_ioctl(3, 1, 0);
	return 0;
}

int module_exit()
{
	remove_kernel_symbol("enumerate_partitions");
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
