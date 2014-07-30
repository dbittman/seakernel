#include <sea/types.h>
#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/tm/process.h>
#include <sea/sys/stat.h>
#include <sea/dm/dev.h>
#include <sea/loader/module.h>
#include <sea/mm/swap.h>
#include <sea/cpu/processor.h>
#include <sea/dm/block.h>
#include <sea/fs/proc.h>
int proc_read_int(char *buf, int off, int len);

#if CONFIG_SMP
int proc_cpu(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
	int total_len=0;
	if(rw == READ) {
		cpu_t *c = cpu_get(m);
		cpuid_t *cpuid = &c->cpuid;
		char tmp[256];
		snprintf(tmp, 256, "cpu: %d\n", c->snum, c->flags);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, "\tCPUID: ", total_len, -1, off, len);
		snprintf(tmp, 256, "%s\n", cpuid->manufacturer_string);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		snprintf(tmp, 256, "\tFamily: 0x%X | Model: 0x%X | Stepping: 0x%X | Type: 0x%X \n",  
			cpuid->family, 
			cpuid->model, 
			cpuid->stepping, 
			cpuid->type);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		snprintf(tmp, 256, "\tCache Line Size: %u bytes | Local APIC ID: 0x%X \n", 
			cpuid->cache_line_size, 
			cpuid->lapic_id);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		snprintf(tmp, 256, "\tCPU Brand: %s \n", cpuid->cpu_brand);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		snprintf(tmp, 256, "\t%s %s %s %s %s %s\n", c->flags&CPU_UP ? "up" : "down", 
			c->flags&CPU_RUNNING ? "running" : "frozen", 
			c->flags&CPU_ERROR ? "ERROR" : "\b", 
			c->flags&CPU_SSE ? "sse" : "\b", 
			c->flags&CPU_FPU ? "fpu" : "\b", 
			c->flags&CPU_PAGING ? "paging" : "segmentation");
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
	}
	return total_len;
}
#endif

#if CONFIG_MODULES

int proc_mods(char rw, struct inode *n, int min, char *buf, int off, int len)
{
	if(rw == READ) {
		int total_len=0, total_mem=0;
		int total=0;
		char tmp[128];
		total_len += proc_append_buffer(buf, "NAME\t\t      SIZE DEPENDENCIES\n"
				, 0, -1, off, len);
		int i;
		module_t *mq = modules;
		while(mq) {
			++total;
			total_mem += mq->length;
			snprintf(tmp, 128, "%-16s %6d KB ", mq->name, mq->length/1024);
			total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
			total_len += proc_append_buffer(buf, "\n", total_len, -1, off, len);
			mq=mq->next;
		}
		snprintf(tmp, 128, "TOTAL: %d modules, %d KB\n", total, total_mem/1024);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		return total_len;
	}
	return -EINVAL;
}

#endif

int proc_kern_rw(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
#if CONFIG_SWAP
	swapdev_t *s = swaplist;
#endif
	int total_len=0;
	if(rw == READ) {
		switch(m) {
			case 3:
				/* List swap devices */
#if CONFIG_SWAP
				if(!s)
				{
					total_len += proc_append_buffer(buf, 
						"no swap devices registered\n", total_len, -1, off, len);
					return total_len;
				}
				total_len += proc_append_buffer(buf, 
					"Device\t\t| Bytes Used| Total Size| Usage %\n", total_len
					, -1, off, len);
				while(s)
				{
					char tmp[1024];
					snprintf(tmp, 1024, "%s\t| %7d MB| %7d MB| %3d%%\n", 
						s->node, (s->bytes_used/1024)/1024, (s->size/1024)/1024
						, (s->bytes_used * 100)/s->size);
					total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
					s=s->next;
				}
				return total_len;
#else
				return 0;
#endif
			case 4:
				return proc_read_int(buf, off, len);
#if CONFIG_BLOCK_CACHE
			case 6:
				return dm_proc_read_bcache(buf, off, len);
#endif
		}
	}
	return 0;
}

int proc_rw_mem(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
	if(rw == READ) {
		char tmp[1024];
		snprintf(tmp, 1024, "    TOTAL | FREE\n%9d | %d [%d%% used]\n", (pm_num_pages * PAGE_SIZE) / 1024, ((pm_num_pages-pm_used_pages) * PAGE_SIZE) / 1024, (pm_used_pages * 100)/pm_num_pages);
		return proc_append_buffer(buf, tmp, 0, -1, off, len);
	}
	return 0;
}
