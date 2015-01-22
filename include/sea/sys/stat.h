#ifndef STAT_H
#define STAT_H

#include <sea/fs/stat.h>

struct task_stat {
	unsigned pid, ppid, *waitflag, stime, utime;
	int uid, gid, state;
	unsigned char system;
	int tty;
	char **argv;
	unsigned mem_usage;
	char cmd[128];
};

struct mem_stat {
	unsigned free;
	unsigned total;
	unsigned used;
	float perc;
	
	unsigned km_loc;
	unsigned km_end;
	unsigned km_numindex;
	unsigned km_maxnodes;
	unsigned km_usednodes;
	unsigned km_numslab;
	unsigned km_maxscache;
	unsigned km_usedscache;
	unsigned km_pagesused;
	char km_name[32];
	float km_version;
};


#endif
