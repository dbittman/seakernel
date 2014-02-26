#ifndef FS_H
#define FS_H
#include <types.h>
#include <sys/stat.h>
#include <pipe.h>
#include <sys/fcntl.h>
#include <ll.h>
#include <rwlock.h>


#define FPUT_CLOSE 1


#include <sea/fs/inode.h>
#include <sea/fs/file.h>

int vfs_callback_read (struct inode *i, off_t a, size_t b, char *d);
int vfs_callback_write (struct inode *i, off_t a, size_t b, char *d);
int vfs_callback_select (struct inode *i, unsigned int m);
struct inode *vfs_callback_create (struct inode *i,char *d, mode_t m);
struct inode *vfs_callback_lookup (struct inode *i,char *d);
struct inode *vfs_callback_readdir (struct inode *i, unsigned n);
int vfs_callback_link (struct inode *i, char *d);
int vfs_callback_unlink (struct inode *i);
int vfs_callback_rmdir (struct inode *i);
int vfs_callback_sync_inode (struct inode *i);
int vfs_callback_unmount (struct inode *i, unsigned int n);
int vfs_callback_fsstat (struct inode *i, struct posix_statfs *s);
int vfs_callback_fssync (struct inode *i);
int vfs_callback_update (struct inode *i);
int do_iremove(struct inode *i, int flag, int);


struct inode *do_get_idir(char *path, struct inode *b, int, int, int *);
int iput(struct inode *i);

int sys_chdir(char *n, int fd);
int ichdir(struct inode *i);
int sys_sync();
int sys_isatty(int f);
int sys_dirstat(char *dir, unsigned num, char *namebuf, struct stat *statbuf);
int sys_getpath(int f, char *b, int);
int sys_ioctl(int fp, int cmd, long arg);
int sys_open(char *name, int flags);
int sys_open_posix(char *name, int flags, mode_t mode);
int sys_close(int fp);
int sys_read(int fp, off_t off, char *buf, size_t count);
int sys_write(int fp, off_t off, char *buf, size_t count);
int sys_seek(int fp, off_t pos, unsigned);
int sys_dup(int f);
int sys_dup2(int f, int n);
int sys_fstat(int fp, struct stat *sb);
int sys_stat(char *f, struct stat *statbuf, int);
int sys_mount(char *node, char *to);
int sys_readpos(int fp, char *buf, size_t count);
int sys_writepos(int fp, char *buf, size_t count);
int sys_mknod(char *path, mode_t mode, dev_t dev);
int sys_chmod(char *path, int, mode_t mode);
int sys_access(char *path, mode_t mode);
int sys_umask(mode_t mode);
int sys_link(char *s, char *d);
int sys_fsync(int f);
int sys_mount2(char *node, char *to, char *name, char *opts, int);
int sys_getdepth(int fd);
int sys_getcwdlen();
int sys_fcntl(int filedes, int cmd, long attr1, long attr2, long attr3);
int sys_symlink(char *p1, char *p2);
int sys_readlink(char *_link, char *buf, int nr);
int sys_dirstat_fd(int fd, unsigned num, char *namebuf, struct stat *statbuf);


int sync_inode_tofs(struct inode *i);
int do_add_inode(struct inode *b, struct inode *i, int);
int remove_inode(struct inode *b, char *name);
int get_path_string(struct inode *p, char *path, int);
int do_chdir(struct inode *);
int do_chroot(struct inode *);
int chdir(char *);
int sys_ftruncate(int f, off_t length);
int sys_getnodestr(char *path, char *node);
int chroot(char *);
int sys_chown(char *path, int, uid_t uid, gid_t gid);
int sys_utime(char *path, time_t a, time_t m);
int get_pwd(char *buf, int);
int unlink(char *f);
int do_fs_stat(struct inode *i, struct fsstat *f);
int rename(char *f, char *nname);
int do_unlink(struct inode *i);
struct inode *read_dir(char *, int num);
int mount(char *d, struct inode *p);
int link(char *old, char *n);
int create_node(struct inode *i, char *name, mode_t mode, int maj, int min);
int write_fs(struct inode *i, off_t off, size_t len, char *b);
int read_fs(struct inode *i, off_t off, size_t len, char *b);
int unmount(char *n, int);
int do_unmount(struct inode *i, int);
int get_ref_count(struct inode *i);
int s_mount(char *name, dev_t dev, u64 block, char *fsname, char *no);
int mount(char *d, struct inode *p);
int do_mount(struct inode *i, struct inode *p);
int is_directory(struct inode *i);
int get_ref_count(struct inode *i);
int free_inode(struct inode *i, int);
int remove_inode(struct inode *b, char *name);
struct inode *do_lookup(struct inode *i, char *path, int aut, int ram, int *);
struct inode *lookup(struct inode *i, char *path);
int sync_inode_tofs(struct inode *i);
int rmdir(char *);
int recur_total_refs(struct inode *i);
int permissions(struct inode *, mode_t);
struct inode *create_m(char *, mode_t);
int change_icount(struct inode *i, int c);
void init_flocks(struct inode *i);
struct inode *read_idir(struct inode *i, int num);
int read_data(int fp, char *buf, off_t off, size_t length);


struct inode *devfs_add(struct inode *q, char *name, mode_t mode, int major, int minor);
struct inode *devfs_create(struct inode *base, char *name, mode_t mode);
void devfs_remove(struct inode *i);
int proc_get_major();
int pfs_write(struct inode *i, off_t pos, size_t len, char *buffer);
int pfs_read(struct inode *i, off_t pos, size_t len, char *buffer);
struct inode *pfs_cn(char *name, mode_t mode, int major, int minor);
struct inode *pfs_cn_node(struct inode *to, char *name, mode_t mode, int major, int minor);
int proc_append_buffer(char *buffer, char *data, int off, int len, 
	int req_off, int req_len);
void init_proc_fs();
void init_dev_fs();
struct inode *init_ramfs();
struct inode *rfs_create(struct inode *__p, char *name, mode_t mode);
int rfs_read(struct inode *i, off_t off, size_t len, char *b);
int rfs_write(struct inode *i, off_t off, size_t len, char *b);
extern struct inode *devfs_root, *procfs_root;
extern struct inode *procfs_kprocdir;
extern struct inode *ramfs_root;
extern struct inode *kproclist;

#endif
