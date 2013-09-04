#include <kernel.h>
#include <fs.h>
#include <dev.h>
#include <char.h>

int rand_maj=-1;
int seed=0;
int get_rand(void);
struct inode *df;
int rand1(unsigned int lim);
int rand2(unsigned int lim);
int rand3(unsigned int lim);
int rand_rw(int rw, int min, char *buf, size_t count)
{
	size_t i;
	if(rw == READ)
	{
		for(i=0;i<count;i++)
		{
			unsigned t = (unsigned)get_rand()*get_epoch_time();
			buf[i] = (char)(t);
		}
	}
	return count;
}

int rand_ioctl(int min, int cmd, long arg)
{
	return 0;
}
 
int get_rand(void)
{
	int k, rnd1, rnd2, rnd3, sum1, sum2, sum3;
	sum1 = sum2 = sum3 = 0;
	rnd1 = rand1(~0); // method one
	rnd2 = rand2(~0); // method two
	rnd3 = rand3(~0); // method three
	return rnd1-rnd2+rnd3;
}

long a1;
int rand1(unsigned int lim)
{
	a1 = (a1 * 125) % 2796203;
	return ((a1 % lim) + 1);
}
 
//
// returns random integer from 1 to lim (Gerhard's generator)
//
int rand2(unsigned int lim)
{
	a1 = (a1 * 32719 + 3) % 32749;
	return ((a1 % lim) + 1);
}
 
//
// returns random integer from 1 to lim (Bill's generator)
//
int rand3(unsigned int lim)
{
	static long a = 3;
	a = (((a * 214013L + 2531011L) >> 16) & 32767);
	return ((a % lim) + 1);
}

int module_install()
{
	df=0;
	rand_maj=0;
	rand_maj = set_availablecd(rand_rw, rand_ioctl, 0);
	if(rand_maj == -1)
		return EINVAL;
	df = devfs_add(devfs_root, "random", S_IFCHR, rand_maj, 0);
	seed=get_epoch_time();
	a1=seed;
	return 0;
}

int module_exit()
{
	if(df)
		devfs_remove(df);
	if(rand_maj > 0) unregister_char_device(rand_maj);
	return 0;
}

int module_deps(char *b)
{
	return KVERSION;
}
