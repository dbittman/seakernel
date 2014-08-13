#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/dm/char.h>
#include <sea/cpu/processor.h>
#include <sea/fs/devfs.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/errno.h>
int rand_maj=-1;
int seed=0;
unsigned int use_rdrand=0;
int get_rand(void);
struct inode *df;
int rand1(unsigned int lim);
int rand2(unsigned int lim);
int rand3(unsigned int lim);

void do_xor(unsigned char *data, unsigned char *xor_vals, int length)
{
	int i;
	for(i=0;i<length;i++)
		data[i] ^= xor_vals[i];
}

#define RD_RAND_RETRY_LOOPS 10
int rdrand32(uint32_t *v)
{
	uint32_t rand_val;
	int done=0;
	int i=0;
	while(done == 0 && i < RD_RAND_RETRY_LOOPS) {
		/* derived from intel's published code for RDRAND */
		asm("rdrand %%eax;\
		     mov $1,%%edx;\
		     cmovae %%eax,%%edx;\
		     mov %%edx,%1;\
		     mov %%eax,%0;":"=r"(rand_val),"=r"(done)::"%eax","%edx");
		*v = rand_val;
		i++;
	}
	return i;
}

int get_rdrand_data(uint32_t *buf, int length)
{
	/* function is only called if rdrand exists */
	int i, flag=0;
	for(i=0;i<length;i++) {
		if(rdrand32(&buf[i]) >= RD_RAND_RETRY_LOOPS)
			flag=1;
	}
	return flag;
}

int rand_rw(int rw, int min, char *buf, size_t count)
{
	size_t i;
	if(rw == READ)
	{
		for(i=0;i<count;i++)
		{
			unsigned t = (unsigned)get_rand()*time_get_epoch();
			buf[i] = (char)(t);
		}
#if CONFIG_ARCH == TYPE_ARCH_X86 || CONFIG_ARCH == TYPE_ARCH_X86_64
		if(use_rdrand) {
			unsigned char rdb[count];
			int r = get_rdrand_data((uint32_t *)rdb, count / sizeof(uint32_t));
			if(r)
				printk(0, "[rand]: warning - rdrand failed\n");
			do_xor((unsigned char *)buf, rdb, count);
		}
#endif
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
	rand_maj = dm_set_available_char_device(rand_rw, rand_ioctl, 0);
	if(rand_maj == -1)
		return EINVAL;
	df = devfs_add(devfs_root, "random", S_IFCHR, rand_maj, 0);
	seed=time_get_epoch();
	a1=seed;
	/* check for rdrand */
#if CONFIG_ARCH == TYPE_ARCH_X86 || CONFIG_ARCH == TYPE_ARCH_X86_64
	use_rdrand = primary_cpu->cpuid.features_ecx & (1 << 30) ? 1 : 0;
	if(use_rdrand)
		printk(0, "[rand]: cpu supports rdrand instruction\n");
#endif
	
	return 0;
}

int module_exit()
{
	if(df)
		devfs_remove(df);
	if(rand_maj > 0) dm_unregister_char_device(rand_maj);
	return 0;
}

int module_deps(char *b)
{
	return CONFIG_VERSION_NUMBER;
}
