#ifndef __SEA_FS_FILE_H
#define __SEA_FS_FILE_H

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)

#include <sea/types.h>
#include <sea/tm/process.h>
#include <sea/lib/hash.h>

struct file {
	_Atomic int count;
	int flags;
	off_t pos;
	struct inode * inode;
	struct dirent *dirent;
};

struct filedes {
	struct file *file;
	int num;
	int flags;
	struct hashelem elem;
};

struct file *file_get_ref(struct file *file);
struct file *file_create(struct inode *inode, struct dirent *dir, int flags);
void file_put(struct file *file);
struct file *file_get(int fdnum);
int file_add_filedes(struct file *f, int start);
void file_remove_filedes(struct filedes *f);
int file_close_fd(int fd);
struct filedes *file_get_filedes(int fdnum);
void fs_copy_file_handles(struct process *p, struct process *n);
void file_close_all(void);
void file_close_cloexec(void);

ssize_t fs_file_pread(struct file *file, off_t offset, uint8_t *buffer, size_t length);
ssize_t fs_file_read(struct file *file, uint8_t *buffer, size_t length);
ssize_t fs_file_pwrite(struct file *file, off_t offset, uint8_t *buffer, size_t length);
ssize_t fs_file_write(struct file *file, uint8_t *buffer, size_t length);
struct file *fs_file_open(const char *name, int flags, mode_t mode, int *error);

int sys_write(int fp, off_t pos, unsigned char *buf, size_t count);
int sys_sync();
int sys_read(int fp, off_t off, unsigned char *buf, size_t count);
int sys_readpos(int fp, unsigned char *buf, size_t count);
int sys_writepos(int fp, unsigned char *buf, size_t count);
int sys_isatty(int f);
int sys_ioctl(int fp, int cmd, long arg);
int sys_open(char *name, int flags, mode_t);
int sys_close(int fp);
int sys_seek(int fp, off_t pos, unsigned);
int sys_dup(int f);
int sys_dup2(int f, int n);
int sys_mknod(char *path, mode_t mode, dev_t dev);
int sys_chmod(char *path, int, mode_t mode);
int sys_access(char *path, mode_t mode);
int sys_umask(mode_t mode);
int sys_link(char *s, char *d);
int sys_fsync(int f);
int sys_symlink(char *p1, char *p2);
int sys_readlink(char *_link, unsigned char *buf, int nr);
int sys_ftruncate(int f, off_t length);
int sys_getnodestr(char *path, char *node);
int sys_chown(char *path, int, uid_t uid, gid_t gid);
int sys_utime(char *path, time_t a, time_t m);

#endif

