#ifndef FS_H
#define FS_H
#include <sys/stat.h>
#include <task.h>
#include <pipe.h>
#include <sys/fcntl.h>

#define SEEK_SET (0)
#define SEEK_CUR (1)
#define SEEK_END (2)
#define OTHERMAY_EXEC 1
#define OTHERMAY_WRITE 2
#define OTHERMAY_READ 4
#define GROUPMAY_EXEC 10
#define GROUPMAY_WRITE 20
#define GROUPMAY_READ 40
#define MAY_EXEC 100
#define MAY_WRITE 200
#define MAY_READ 400
extern struct sblktbl *sb_table;
#define INAME_LEN 128
struct inode {
	/* Attributes */
	unsigned short mode, uid, gid, nlink;
	unsigned char unreal, dynamic, required;
	unsigned int flags, len, start, nblocks, ctime, atime, mtime;
	int count, f_count, newlocks;
	/* Identification */
	char name[128];
	unsigned int dev;
	unsigned long num;
	unsigned int sb_idx;
	char node_str[INAME_LEN];
	int devnum;
	/* Pointers */
	struct inode_operations *i_ops;
	struct inode *mount_ptr, *r_mount_ptr;
	struct inode *child;
	struct inode *parent;
	struct inode *next;
	struct inode *prev;
	pipe_t *pipe;
	/* Locking */
	mutex_t lock;
	struct flock *flocks;
	mutex_t *flm;
};

struct sblktbl {
	int version;
	char name[16];
	struct inode * (*sb_load)(int dev, int block, char *);
	struct sblktbl *next, *prev;
};

struct mountlst {
	struct inode *i;
	struct mountlst *next, *prev;
};

struct file {
	unsigned int flags, fd_flags, count, pos;
	struct inode * inode;
};

struct file_ptr {
	unsigned int num;
	struct file *fi;
};

struct inode_operations {
	int (*read) (struct inode *, unsigned int, unsigned int, char *);
	int (*write) (struct inode *, unsigned int, unsigned int, char *);
	int (*select) (struct inode *, unsigned int);
	struct inode *(*create) (struct inode *,char *, unsigned int);
	struct inode *(*lookup) (struct inode *,char *);
	struct inode *(*readdir) (struct inode *, unsigned);
	int (*link) (struct inode *, char *);
	int (*unlink) (struct inode *);
	int (*rmdir) (struct inode *);
	int (*sync_inode) (struct inode *);
	int (*unmount)(struct inode *, unsigned int);
	int (*fsstat)(struct inode *, struct posix_statfs *);
	int (*fssync)(struct inode *);
	int (*update)(struct inode *);
};

int vfs_callback_read (struct inode *i, unsigned int a, unsigned int b, char *d);
int vfs_callback_write (struct inode *i, unsigned int a, unsigned int b, char *d);
int vfs_callback_select (struct inode *i, unsigned int m);
struct inode *vfs_callback_create (struct inode *i,char *d, unsigned int m);
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
int do_iremove(struct inode *i, int flag);

#define iremove_recur(i)  do_iremove(i, 2)
#define iremove(i)        do_iremove(i, 0)
#define iremove_nofree(i) do_iremove(i, 3)
#define iremove_force(i)  do_iremove(i, 1)

#define get_idir(path,in_st) do_get_idir(path, in_st, 0, 0, 0)
#define lget_idir(path,in_st) do_get_idir(path, in_st, 1, 0, 0)
#define clget_idir(path,in_st,x) do_get_idir(path, in_st, 1, x, 0)
#define cget_idir(path,in_st,x) do_get_idir(path, in_st, 1, x, 0)
#define ctget_idir(path,in_st,x,res) do_get_idir(path, in_st, 1, x, res)

int sys_sync();
int sync_inode_tofs(struct inode *i);
int add_inode(struct inode *b, struct inode *i);
int remove_inode(struct inode *b, char *name);
int get_path_string(struct inode *p, char *path);
struct inode *do_get_idir(char *path, struct inode *b, int, int, int *);
int iput(struct inode *i);
int do_chdir(struct inode *);
int do_chroot(struct inode *);
int chdir(char *);
int sys_ftruncate(int f, unsigned length);
int sys_getnodestr(char *path, char *node);
int chroot(char *);
int sys_chown(char *path, int uid, int gid);
int sys_utime(char *path, unsigned a, unsigned m);
int get_pwd(char *buf, int);
int unlink(char *f);
int proc_get_major();
int do_fs_stat(struct inode *i, struct fsstat *f);
int rename(char *f, char *nname);
int sys_isatty(int f);
int sys_dirstat(char *dir, unsigned num, char *namebuf, struct stat *statbuf);
int pfs_write(struct inode *i, unsigned int pos, unsigned int len, char *buffer);
int pfs_read(struct inode *i, unsigned int pos, unsigned int len, char *buffer);
struct inode *create_procfs(struct inode *i, char *c, int h);
struct inode *pfs_cn(char *name, int mode, int major, int minor);
void remove_dfs_node(char *name);
int sys_getpath(int f, char *b);
struct inode *read_dir(char *, int num);
int mount(char *d, struct inode *p);
struct inode *dfs_cn(char *name, int mode, int major, int minor);
int link(char *old, char *new);
int create_node(struct inode *i, char *name, int mode, int maj, int min);
int write_fs(struct inode *i, int off, int len, char *b);
int read_fs(struct inode *i, int off, int len, char *b);
int unmount(char *n, int);
int do_unmount(struct inode *i, int);
int block_ioctl(int dev, int cmd, int arg);
int char_ioctl(int dev, int cmd, int arg);
int dm_ioctl(int type, int dev, int cmd, int arg);
int do_fs_stat(struct inode *i, struct fsstat *f);
int fs_stat(char *path, struct fsstat *f);
int sys_fsstat(int fp, struct fsstat *fss);
int sys_ioctl(int fp, int cmd, int arg);
int sys_open(char *name, int flags);
struct file *d_sys_open(char *name, int flags, int mode, int *, int *);
int sys_open_posix(char *name, int flags, int mode);
int sys_close(int fp);
int sys_read(int fp, unsigned off, char *buf, unsigned count);
int sys_write(int fp, unsigned off, char *buf, unsigned count);
int sys_seek(int fp, unsigned pos, unsigned);
int sys_dup(int f);
int sys_dup2(int f, int n);
int sys_fstat(int fp, struct stat *sb);
int sys_stat(char *f, struct stat *statbuf, int);
void add_mountlst(struct inode *n);
void remove_mountlst(struct inode *n);
void unmount_all();
int register_sbt(char *name, int ver, int (*sbl)(int,int,char *));
struct inode *sb_callback(char *fsn, int dev, int block, char *n);
struct inode *sb_check_all(int dev, int block, char *n);
int unregister_sbt(char *name);
int execve(char *path, char **argv, char **env);
int load_superblocktable();
int get_ref_count(struct inode *i);
int sys_mount(char *node, char *to);
int s_mount(char *name, int dev, int block, char *fsname, char *no);
int mount(char *d, struct inode *p);
int do_mount(struct inode *i, struct inode *p);
int sys_readpos(int fp, char *buf, unsigned count);
int sys_writepos(int fp, char *buf, unsigned count);
int is_directory(struct inode *i);
int get_ref_count(struct inode *i);
int permissions(struct inode *inode, int flag);
int add_inode(struct inode *b, struct inode *i);
int free_inode(struct inode *i, int);
int remove_inode(struct inode *b, char *name);
struct inode *do_lookup(struct inode *i, char *path, int aut, int ram);
struct inode *lookup(struct inode *i, char *path);
int sys_mknod(char *path, unsigned mode, unsigned dev);
int sys_chmod(char *path, int mode);
int sys_access(char *path, int mode);
struct inode *sys_getidir(char *path, int fd);
int sys_umask(int mode);
struct inode *sys_create(char *path);
int sys_link(char *s, char *d);
int sys_fsync(int f);
int sync_inode_tofs(struct inode *i);
int rmdir(char *);
int sys_sbrk(int inc);
int sys_mount2(char *node, char *to, char *name, char *opts, int);
extern struct inode *ramfs_root;
void init_dev_fs();
void init_proc_fs();
int sys_posix_fsstat(int fd, struct posix_statfs *sb);
int sys_sync();
struct inode *init_ramfs();
struct inode *rfs_create(struct inode *__p, char *name, unsigned int mode);
int rfs_read(struct inode *i, unsigned int off, unsigned int len, char *b);
int rfs_write(struct inode *i, unsigned int off, unsigned int len, char *b);
int recur_total_refs(struct inode *i);
extern struct inode *devfs_root, *procfs_root;
int sys_fcntl(int filedes, int cmd, int attr1, int attr2, int attr3);
int permissions(struct inode *, int);
struct inode *create_m(char *, int);
extern mutex_t vfs_mutex;
int sys_symlink(char *p1, char *p2);
int sys_readlink(char *_link, char *buf, int nr);
int change_icount(struct inode *i, int c);
extern struct inode *kproclist;
void init_flocks(struct inode *i);
int do_sys_write_flags(struct file *f, unsigned off, char *buf, unsigned count);
int do_sys_read_flags(struct file *f, unsigned off, char *buf, unsigned count);
#endif
