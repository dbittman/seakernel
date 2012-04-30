#include <kernel.h>
#include <fs.h>
#include <sys/stat.h>
#include <dev.h>
#include <cache.h>
#include <block.h>
#include <mod.h>
int NUM_LOOP = 2;
int loop_maj = -1;
char loop_table[20][128];
struct inode *loop_f[20];
int loop_rw(int rw, int minor, int block, char *buf)
{
	int ret = 0;
	int f;
	if(!loop_f[minor])
		return -EINVAL;
	if(rw == READ)
		ret = read_fs(loop_f[minor], block*512, 512, buf);
	else if(rw == WRITE)
		ret = write_fs(loop_f[minor], block*512, 512, buf);
	return ret;
}

int loop_up(int num, char *name)
{
	if(num >= 20)
		return -EINVAL;
	if(loop_table[num][0] != 0)
		return -EEXIST;
	strcpy(loop_table[num], name);
	loop_f[num] = get_idir(name, 0);
	if(!loop_f[num])
		return -ENOENT;
	return 0;
}

int loop_down(int num)
{
	if(num >= 20)
		return -EINVAL;
	if(!loop_f[num]) return -ENOENT;
	disconnect_block_cache(GETDEV(loop_maj, num));
	memset(loop_table[num], 0, 128);
	iput(loop_f[num]);
	loop_f[num]=0;
	return 0;
}

int ioctl_main(int min, int cmd, int arg)
{
	if(cmd == 0)
		return loop_up(min, (char *)arg);
	else if(cmd == 1)
		return loop_down(min);
	return 0;
}

int strtoint(char *s)
{
	int i;
	int dig = strlen(s);
	int mul=1;
	int total=0;
	char neg=0;
	if(*s == '-')
	{
		neg = 1;
		s++;
	}
	for(i=0;i<(dig-1);i++) mul *= 10;
	while(*s)
	{
		if(*s < 48 || *s > 57)
			return -1;
		total += ((*s)-48)*mul;
		mul = mul/10;
		s++;
	}
	if(neg) return (-total);
	return total;
}

int module_install()
{
	int f = sys_open("/config/loopconfig", O_RDWR);
	if(f > 0)
	{
		struct stat sf;
		sys_fstat(f, &sf);
		int len = sf.st_size;
		char tmp[len+1];
		memset(tmp, 0, len+1);
		sys_read(f, 0, tmp, len);
		sys_close(f);
		NUM_LOOP = strtoint(tmp);
	}
	if(NUM_LOOP > 19) NUM_LOOP=19;
	printk(KERN_DEBUG, "[loop]: Creating %d loopback devices...\n", NUM_LOOP);
	loop_maj = set_availablebd(loop_rw, 512, ioctl_main, 0);
	int i;
	char name[8];
	for(i=0;i<NUM_LOOP;i++)
	{
		sprintf(name, "loop%d", i);
		dfs_cn(name, S_IFBLK, loop_maj, i);
		memset(loop_table[i], 0, 128);
	}
	memset(loop_f, 0, 19);
	return 0;
}

int module_exit()
{
	unregister_block_device(loop_maj);
	int i;
	char name[8];
	for(i=0;i<NUM_LOOP;i++)
	{
		loop_down(i);
		sprintf(name, "loop%d", i);
		remove_dfs_node(name);
	}
	
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
