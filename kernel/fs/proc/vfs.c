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
int proc_append_buffer(char *buffer, char *data, int off, int len, int req_off, 
	int req_len);
struct inode *get_sb_table(int n);

struct mnttab {
	char	*mt_special;
	char	*mt_mountp;
	char	*mt_filsys;
	char	*mt_mntopts;
	char	*mt_time;
	char *mt_dev;
};

int proc_vfs(char rw, struct inode *n, int m, char *buf, int off, int len)
{
	int total_len=0;
	if(m == 1)
	{
		struct inode *i;
		int c=0;
		while((i=get_sb_table(c++)))
		{
			if(!strcmp(i->mount_parent->name, "dev"))
				continue;
			if(!strcmp(i->mount_parent->name, "proc"))
				continue;
			if(!strcmp(i->mount_parent->name, "tmp"))
				continue;
			struct mnttab mt;
			mt.mt_special = "";
			mt.mt_mountp = "";
			mt.mt_filsys = "";
			mt.mt_mntopts = "rw";
			mt.mt_time = "0";
			mt.mt_dev = "";
			if(!i->node_str[0])
			{
				if(i->mount && i->mount->root)
					mt.mt_dev = i->mount->root->name;
				else
					mt.mt_dev = i->name;
			} else
				mt.mt_dev = strrchr(i->node_str, '/')+1;
			if(i->mount_parent) {
				if(i->mount_parent == current_task->thread->root || i == current_task->thread->root)
					mt.mt_filsys=mt.mt_mountp = "/";
				else {
					if(!strcmp(i->mount_parent->name, "dev"))
						mt.mt_mountp = "/dev";
					if(!strcmp(i->mount_parent->name, "tmp"))
						mt.mt_mountp = "/tmp";
					if(!strcmp(i->mount_parent->name, "proc"))
						mt.mt_mountp = "/proc";
				}
			}
			total_len += proc_append_buffer(buf, (char *)&mt, total_len, 
				sizeof(mt), off, len);
		}
	}
	return total_len;
}
