#ifndef __SEA_FS_KERFS
#define __SEA_FS_KERFS

#include <sea/fs/inode.h>
#include <sea/vsprintf.h>
#include <sea/string.h>

#define KERFS_PARAM 1
#define KERFS_PARAM_WRITE 4

void kerfs_init();
int kerfs_register_parameter(char *path, void *param, size_t size,
		int flags, int (*)(int, void *, size_t, size_t, size_t, unsigned char *));
int kerfs_unregister_entry(char *path);

int kerfs_syscall_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_int_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_kmalloc_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_pmm_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_icache_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_module_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_route_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_mount_report(int, void *, size_t, size_t, size_t, unsigned char *);
int kerfs_pfault_report(int direction, void *param, size_t size, size_t offset, size_t length, unsigned char *buf);
int kerfs_syslog(int direction, void *param, size_t size, size_t offset, size_t length, unsigned char *buf);
int kerfs_block_cache_report(int direction, void *param, size_t size,
		size_t offset, size_t length, unsigned char *buf);
int kerfs_frames_report(int direction, void *param, size_t size, size_t offset, size_t length, unsigned char *buf);

int kerfs_rw_string(int direction, void *param, size_t sz,
		size_t offset, size_t length, unsigned char *buffer);
int kerfs_rw_address(int direction, void *param, size_t sz,
		size_t offset, size_t length, unsigned char *buffer);
int kerfs_rw_integer(int direction, void *param, size_t sz, size_t offset, size_t length,
		unsigned char *buffer);

#define kerfs_register_report(path, fn) \
	kerfs_register_parameter(path, NULL, 0, 0, fn)

#define KERFS_PRINTF(offset,length,buf,current,format...) \
	do { \
		char line[1024]; \
		int add = snprintf(line, 1024, format); \
		if(current + add > offset && current < (offset + length)) { \
			size_t linestart = current > offset ? 0 : (offset - current); \
			size_t bufstart  = current > offset ? (current - offset) : 0; \
			size_t amount = add - linestart; \
			if(amount > ((offset + length) - current)) \
				amount = (offset + length) - current; \
			memcpy(buf + bufstart, line + linestart, amount); \
			current += amount; \
		} else if(current + add <= offset) { \
			offset -= add; \
		} \
	} while(0);

#endif

