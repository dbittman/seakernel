#ifndef STAT_H
#define STAT_H
typedef signed long time_t;
#define S_IFMT  00170000
#define	S_IFSOCK 0140000	/* socket */
#define	S_IFLNK  0120000
#define S_IFREG  0100000
#define S_IFBLK  0060000
#define S_IFDIR  0040000
#define S_IFCHR  0020000
#define S_IFIFO  0010000
#define S_ISUID  0004000
#define S_ISGID  0002000
#define S_ISVTX  0001000

#define S_ISREG(m)	(((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m)	(((m) & S_IFMT) == S_IFDIR)
#define S_ISCHR(m)	(((m) & S_IFMT) == S_IFCHR)
#define S_ISBLK(m)	(((m) & S_IFMT) == S_IFBLK)
#define S_ISFIFO(m)	(((m) & S_IFMT) == S_IFIFO)
#define S_ISLNK(m)	(((m) & S_IFMT) == S_IFLNK)
#define	S_ISSOCK(m)	(((m)&_IFMT) == _IFSOCK)

struct task_stat {
	unsigned pid, ppid, *waitflag, stime, utime;
	int uid, gid, state;
	unsigned char system;
	int tty;
	struct inode *exe;
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

struct stat {
	unsigned short	st_dev;
	unsigned long	st_ino;
	unsigned short	st_mode;
	unsigned short	st_nlink;
	unsigned short	st_uid;
	unsigned short	st_gid;
	unsigned short	st_rdev;
	unsigned long	st_size;
	unsigned long	st_blksize;
	unsigned long	st_blocks;

		time_t	st_atime;
  long		st_spare1;
  time_t	st_mtime;
  long		st_spare2;
  time_t	st_ctime;
};

struct fsstat {
		unsigned f_type;
    unsigned f_bsize;
    unsigned long long f_blocks;
    unsigned long long f_bfree;
    unsigned long long f_bavail;
    unsigned long long f_files;
    unsigned long long f_ffree;
    unsigned f_fsid;
    unsigned f_namelen;
    unsigned f_frsize;
    unsigned f_flags;
    unsigned f_spare[4];
	
	unsigned long total_size;
	unsigned long used_size, free_size;
	unsigned long dev;
	char name[8];
};

struct posix_statfs
{
    unsigned f_type;
    unsigned f_bsize;
    unsigned long long f_blocks;
    unsigned long long f_bfree;
    unsigned long long f_bavail;
    unsigned long long f_files;
    unsigned long long f_ffree;
    unsigned f_fsid;
    unsigned f_namelen;
    unsigned f_frsize;
    unsigned f_flags;
    unsigned f_spare[4];
};

#endif
