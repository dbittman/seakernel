/* fat - Provides access to fat[12/16/32] filesystems. */
#include <kernel.h>
#include <task.h>
#include <dev.h>
#include <fs.h>
#include "fat.h"

struct file_operations fatfs_fops = {
	0,
	0,//wrap_ext2_readfile,
	0,//wrap_ext2_writefile,
	0,
	0,//ext2_stat,
	0,
	0,
	0,
	0,
	0,
	0
};

struct inode_operations fatfs_inode_ops = {
	&fatfs_fops,
	0,//wrap_ext2_create,
	0,//wrap_ext2_lookup,
	0,
	0,//wrap_ext2_readdir,
	0,//(int (*) (struct inode *,const char *))wrap_ext2_link,
	0,//wrap_ext2_unlink,
	0,//wrap_ext2_readdir_name,
	0,
	0,//wrap_ext2_unlink,//seperate
	0,
	0,
	0,//wrap_put_inode,
	0,//ext2_sane,//sane
	0,//ext2_fs_sane,//fssane
	0,//wrap_sync_inode,
	0,
	0,//ext2_unmount,
	0,//ext2_fs_stat,
	0,//ext2_sync,
	0,//wrap_ext2_update,
};

fat_volume vols[MAX_VOLS];
fat_volume *get_new_vol()
{
	int i;
	for(i=0;i<MAX_VOLS;i++)
	{
		if(vols[i].vid==-1) {
			vols[i].vid=i;
			vols[i].ext16=0;
			vols[i].ext32=0;
			return &vols[i];
		}
	}
	return 0;
}

void release_vol(fat_volume *fs)
{
	kfree(fs->bpb);
	fs->vid=-1;
}

int fat_read_block(fat_volume *fs, unsigned block, unsigned char *buf)
{
	int off = block * fs->bpb->bytes_per_sector + fs->block*512;
	return block_device_rw(READ, fs->dev, off, (char *)buf, fs->bpb->bytes_per_sector);
}

struct inode *create_sea_inode(fat_volume *fs, fat_dirent *in)
{
	if(!in) return 0;
	struct inode *out = (struct inode *)kmalloc(sizeof(struct inode));
	if(!out)
		return 0;
	out->mode = 0x1FF;
	if(in->attr & 0x10)
		out->mode |= S_IFDIR;
	else
		out->mode |= S_IFREG;
	//out->uid = 0;
	out->len = in->size;
	//out->atime = in->access_time;
	//out->mtime = in->modification_time;
	//out->gid = in->gid;
	//out->nlink = in->link_count;
	//out->num = in->number;
	//out->nblocks = in->sector_count;
	out->sb_idx = fs->vid;
	out->dynamic=1;
	out->flm = create_mutex(0);
	out->i_ops = &fatfs_inode_ops;
	create_mutex(&out->lock);
	strncpy(out->name, (char *)in->name, 8);
	strcat(out->name, ".");
	strncat(out->name, (char *)in->ext, 3);
	
	unsigned start = (in->start_high << 16) | (in->start_low & 0x0000FFFF);
	out->num=start;
	return out;
}

struct inode *fat_mount(int dev, int block, char *node)
{
	fat_volume *fs = get_new_vol();
	if(!fs) {
		printk(5, "[fat]: Unable to allocate new filesystem!\n");
		return 0;
	}
	struct inode *in = get_idir(node, 0);
	if(in && dev == -1)
		dev = in->dev;
	if(in && (int)in->dev != dev)
		printk(4, "[fat]: Odd...node device is different from given device...\n");
	iput(in);
	fs->block=block;
	fs->dev=dev;
	fs->bpb = (bpb_t *)kmalloc(512);
	/* Load the bpb */
	fat_read_block(fs, 0, (unsigned char *)fs->bpb);
	/* Make sure its FAT */
	
	/* Calculate some useful values */
	char probably_32=0;
	if(*(unsigned char *)((unsigned char*)(fs->bpb) + 66) == 0x28 
			|| *(unsigned char *)((unsigned char*)(fs->bpb) + 66) == 0x29) {
		probably_32=1;
		fs->ext32=(fat_32_extbpb *)(((unsigned char*)fs->bpb)+36);
	}
	if(!probably_32)
	{
		if(!(*(unsigned char *)((unsigned char*)(fs->bpb) + 38) == 0x28 
			|| *(unsigned char *)((unsigned char*)(fs->bpb) + 38) == 0x29))
		{
			printk(0, "[fat]: Not a FAT volume!\n");
			goto error;
		}
	}
	fs->root_dir_sec = ((fs->bpb->num_dirent * 32) + 
		(fs->bpb->bytes_per_sector - 1)) / fs->bpb->bytes_per_sector;
	fs->num_data_sec = TOTAL_SECTORS(fs) - 
		(fs->bpb->res_sectors + (fs->bpb->num_fats * (!probably_32 
		? fs->bpb->sec_per_fat : fs->ext32->sec_per_fat)) + fs->root_dir_sec);
	
	fs->first_data_sec = fs->bpb->res_sectors + 
		(fs->bpb->num_fats * (!probably_32 
		? fs->bpb->sec_per_fat : fs->ext32->sec_per_fat));
	
	fs->fat_type = fat_type(NUM_CLUSTERS(fs));
	if(fs->fat_type == FAT32 && !probably_32)
	{
		printk(5, "[fat]: Unable to assume FAT 32 even though it looks like it is\n");
		goto error;
	}
	if(fs->fat_type == FAT16 || fs->fat_type == FAT12)
		fs->ext16 = (fat_16_extbpb *)(((unsigned char*)fs->bpb)+36);
	
	/* Load root directory and create sea inode */
	if(fs->fat_type != FAT32)
	{
		printk(5, "[fat]: Don't support fat 12 or 16 yet\n");
		goto error;
	}
	
	struct inode *out = (struct inode *)kmalloc(sizeof(struct inode));
	if(!out)
		return 0;
	out->mode = 0x1FF;
	out->mode |= S_IFDIR;
	
	out->len = 0;
	
	out->sb_idx = fs->vid;
	out->dynamic=1;
	out->flm = create_mutex(0);
	out->i_ops = &fatfs_inode_ops;
	create_mutex(&out->lock);
	
	strncpy(out->name, (char *)"fat", 3);
	
	out->num=(fs->fat_type == FAT32 ? fs->ext32->root_cluster : fs->first_data_sec);
	printk(1, "[fat]: Mounted successfully\n");
	return out;
	
	error:
	release_vol(fs);
	return 0;
}

int module_install()
{
	int i;
	for(i=0;i<MAX_VOLS;i++)
		vols[i].vid=-1;
	register_sbt("fat", 1, (int (*)(int,int,char*))fat_mount);
	return 0;
}

int module_exit()
{
	unregister_sbt("ext2");
	return 0;
}
int module_deps(char *b)
{
	return KVERSION;
}
