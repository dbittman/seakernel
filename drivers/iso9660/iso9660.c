#include <kernel.h>
#include <fs.h>
#include <sys/stat.h>
#include "iso9660.h"
int iso9660_unmount(unsigned  v);
iso_fs_t *get_fs(int v);
int lowercase=1;
struct inode *create_sea_inode(iso_fs_t *fs, struct iso9660DirRecord *in, char *name);
int iso_read_block(iso_fs_t *fs, unsigned block, unsigned char *buf)
{
	int off = block * 2048 + fs->block*2048;
	return block_device_rw(READ, fs->dev, off, (char *)buf, 2048);
}

int iso_read_off(iso_fs_t *fs, unsigned off, unsigned char *buf, unsigned len)
{
	off += fs->block*2048;
	return block_device_rw(READ, fs->dev, off, (char *)buf, len);
}

struct inode *wrap_iso_readdir(struct inode *in, unsigned  num)
{
	num+=2;
	iso_fs_t *fs = get_fs(in->sb_idx);
	if(!fs)
		return 0;
	struct iso9660DirRecord *file = (struct iso9660DirRecord *)in->start;
	if(!file)
		return 0;
	struct iso9660DirRecord find;
	char name[128];
	memset(name, 0, 128);
	int res = read_dir_rec(fs, file, num, &find, name);
	if(res == -1)
		return 0;
	struct inode *ret = create_sea_inode(fs, &find, name);
	ret->parent=in;
	return ret;
}

struct inode *wrap_iso_lookup(struct inode *in, char *name)
{
	iso_fs_t *fs = get_fs(in->sb_idx);
	if(!fs) {
		return 0;
	}
	struct iso9660DirRecord *file = (struct iso9660DirRecord *)in->start;
	if(!file) {
		return 0;
	}
	struct iso9660DirRecord find;
	int res = search_dir_rec(fs, file, name, &find);
	if(res == -1) {
		return 0;
	}
	struct inode *ret = create_sea_inode(fs, &find, name);
	ret->parent = in;
	return ret;
}

int wrap_iso_readfile(struct inode *in, unsigned int off, unsigned int len, char *buf)
{
	iso_fs_t *fs = get_fs(in->sb_idx);
	if(!fs)
		return -ENOENT;
	struct iso9660DirRecord *file = (struct iso9660DirRecord *)in->start;
	if(!file)
		return -EINVAL;
	return iso9660_read_file(fs, file, buf, off, len);
}

struct inode_operations iso9660_inode_ops = {
	wrap_iso_readfile,
	0, 0,
	0,
	wrap_iso_lookup,
	wrap_iso_readdir,
	0,
	0,
	0,
	0,
	iso9660_unmount,
	0, 0, 0
};

iso_fs_t vols[MAX_ISO];
iso_fs_t *get_new_fsvol()
{
	int i;
	for(i=0;i<MAX_ISO;i++)
		if(vols[i].flag == -1)
			break;
	if(i == MAX_ISO)
		return 0;
	vols[i].flag=i;
	vols[i].pvd = (struct iso9660pvd *)kmalloc(2048);
	return &vols[i];
}

iso_fs_t *get_fs(int v)
{
	if(v < 0 || v > MAX_ISO) return 0;
	return &vols[v];
}

void release_fsvol(iso_fs_t *fs)
{
	if(!fs) return;
	if(fs->flag == -1)
		return;
	fs->flag=-1;
	kfree(fs->pvd);
}
 inline char isLeap(uint32_t year)
  {
    if(year % 400 == 0) return 1;
    else if(year % 100 == 0) return 0;
    else if(year % 4 == 0) return 1;
    else return 0;
  }
/* Borrow from pedigree, will rewrite */
  unsigned long timeToUnix(struct Iso9660DirTimestamp time)
  {
   unsigned long ret = 0;

    ret += time.Second;
    ret += time.Minute * 60;
    ret += time.Hour * 60 * 60;
    ret += (time.Day - 1) * 24 * 60 * 60;

    static uint16_t cumulativeDays[] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365};
    ret += cumulativeDays[time.Month - 1] * 24 * 60 * 60;

    uint32_t year = time.Year + 1900;
    if((time.Month) > 2 && isLeap(year))
      ret += 24 * 60 * 60;

    // Add leap days
    uint32_t realYear = year;
    uint32_t leapDays = ((realYear / 4) - (realYear / 100) + (realYear / 400));
    leapDays -= ((1970 / 4) - (1970 / 100) + (1970 / 400));

    ret += leapDays * 24 * 60 * 60;
    ret += (year - 1970) * 365 * 24 * 60 * 60;

    return ret;
  }



struct inode *create_sea_inode(iso_fs_t *fs, struct iso9660DirRecord *in, char *name)
{
	if(!in) return 0;
	struct inode *out = (struct inode *)kmalloc(sizeof(struct inode));
	if(!out)
		return 0;
	/* Convert to unix file mode */
	if(in->FileFlags & 0x2)
		out->mode |= 0x4000;
	else
		out->mode |= 0x8000;
	out->mode |= 0x1FF;
	out->len = in->DataLen_LE;
	out->atime = get_epoch_time();
	out->mtime = timeToUnix(in->Time);
	out->nlink = 1;
	out->nblocks = out->len / 2048;
	out->sb_idx = fs->flag;
	out->dynamic=1;
	out->flm = create_mutex(0);
	out->i_ops = &iso9660_inode_ops;
	out->start = kmalloc(in->RecLen + 128);
	memcpy((void *)out->start, in, in->RecLen);
	create_mutex(&out->lock);
	int i;
	memset(out->name, 0, 64);
	/* Modify name */
	if(lowercase)
	{
		for(i=0;i<63;i++) {
			if(!name[i])
				break;
			if(name[i] >= 'A' && name[i] <= 'Z')
				out->name[i] = name[i] + 32;
			else
				out->name[i] = name[i];
		}
		
	} else
		strncpy(out->name, name, 128);
	return out;
}

struct inode *iso9660_mount(int dev, int block, char *node)
{
	int sector=0x10;
	unsigned char buf[2048];
	iso_fs_t tmp;
	tmp.dev=dev;
	tmp.block=block;
	iso_pvd_t_blank *pvd;
	while(sector < 0x20)
	{
		iso_read_block(&tmp, sector, buf);
		voldesc_t *vd = (voldesc_t *)buf;
		if(strncmp(vd->id, "CD001", 5))
			return 0;
		if(vd->type == 255)
		{
			return 0;
		}
		else if(vd->type == 1)
		{
			pvd = (iso_pvd_t_blank *)buf;
			break;
		}
		sector++;
	}
	iso_fs_t *fs = get_new_fsvol();
	if(!fs)
		return 0;
	fs->dev = dev;
	fs->block=block;
	memset(fs->node, 0, 16);
	strncpy(fs->node, node, 16);
	memcpy(fs->pvd, pvd, 2048);
	struct iso9660DirRecord *rd = get_root_dir(fs);
	struct inode *root = create_sea_inode(fs, rd, "iso9660");
	root->mode = 0x4000 | 0xFFF;
	if(!root) return 0;
	fs->root=root;
	strcpy(fs->root->node_str, node);
	return root;
}

int iso9660_unmount(unsigned int v)
{
	iso_fs_t *fs = get_fs(v);
	if(!fs)
		return -1;
	struct inode *root = fs->root;
	release_fsvol(fs);
	return 0;
}

int module_install()
{
	int i=0;
	for(i=0;i<MAX_ISO;i++)
		vols[i].flag=-1;
	register_sbt("iso9660", 1, (int (*)(int,int,char*))iso9660_mount);
	return 0;
}

int module_exit()
{
	int i=0;
	for(i=0;i<MAX_ISO;i++)
		iso9660_unmount(i);
	return 0;
}
int module_deps(char *b)
{
	return KVERSION;
}
