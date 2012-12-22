#include <types.h>
#include <kernel.h>
#include <fs.h>
#include <task.h>
#include <sys/stat.h>
#include <dev.h>
#include <mod.h>
#include <swap.h>
#include <cpu.h>
extern struct inode *procfs_root, *procfs_kprocdir;
int proc_read_int(char *buf, int off, int len);
int proc_read_mutex(char *buf, int off, int len);
int proc_read_bcache(char *buf, int off, int len);
int proc_append_buffer(char *buffer, char *data, int off, int len, 
		int req_off, int req_len);

#if CONFIG_SMP
int proc_cpu(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
	int total_len=0;
	if(rw == READ) {
		cpu_t *c = get_cpu(m);
		cpuid_t *cpuid = &c->cpuid;
		char tmp[256];
		sprintf(tmp, "cpu: %d\n", c->apicid, c->flags);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, "\tCPUID: ", total_len, -1, off, len);
		sprintf(tmp, "%s\n", cpuid->manufacturer_string);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		sprintf(tmp, "\tFamily: 0x%X | Model: 0x%X | Stepping: 0x%X | Type: 0x%X \n",  
			cpuid->family, 
			cpuid->model, 
			cpuid->stepping, 
			cpuid->type);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		sprintf(tmp, "\tCache Line Size: %u bytes | Local APIC ID: 0x%X \n", 
			cpuid->cache_line_size, 
			cpuid->lapic_id);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		sprintf(tmp, "\tCPU Brand: %s \n", cpuid->cpu_brand);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		sprintf(tmp,"\t%s %s %s %s %s %s\n", c->flags&CPU_UP ? "up" : "down", 
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
			sprintf(tmp, "%-16s %6d KB ", mq->name, mq->length/1024);
			total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
			total_len += proc_append_buffer(buf, mq->deps, total_len, -1, off, len);
			total_len += proc_append_buffer(buf, "\n", total_len, -1, off, len);
			mq=mq->next;
		}
		sprintf(tmp, "TOTAL: %d modules, %d KB\n", total, total_mem/1024);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		return total_len;
	}
	return -EINVAL;
}

#endif

int proc_kern_rw(char rw, struct inode *inode, int m, char *buf, int off, int len)
{
	swapdev_t *s = swaplist;
	int total_len=0;
	if(rw == READ) {
		switch(m) {
			case 3:
				/* List swap devices */
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
					sprintf(tmp, "%s\t| %7d MB| %7d MB| %3d%%\n", 
						s->node, (s->bytes_used/1024)/1024, (s->size/1024)/1024
						, (s->bytes_used * 100)/s->size);
					total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
					s=s->next;
				}
				return total_len;
			case 4:
				return proc_read_int(buf, off, len);
			case 6:
				return proc_read_bcache(buf, off, len);
		}
	}
	return 0;
}
