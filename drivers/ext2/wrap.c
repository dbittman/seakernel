#include <modules/ext2.h>
#include <task.h>
int wrap_ext2_update(struct inode *i);
struct inode *wrap_ext2_lookup(struct inode *in, char *name);
int wrap_ext2_readfile(struct inode *in, off_t off, size_t len, char *buf);
int wrap_ext2_writefile(struct inode *in, off_t off, size_t len, char *buf);
struct inode *wrap_ext2_readdir(struct inode *node, unsigned num);
int copyto_ext2_inode(struct inode *out, ext2_inode_t *in);
int update_sea_inode(struct inode *out, ext2_inode_t *in, char *name);
int wrap_ext2_unlink(struct inode *i);
int wrap_ext2_link(struct inode *i, char *path);
struct inode *wrap_ext2_create(struct inode *i, char *name, mode_t mode);
int ext2_unmount(struct inode *, unsigned v);
int wrap_sync_inode(struct inode *i);
int ext2_fs_stat(struct inode *i, struct posix_statfs *f);
int ext2_dir_get_inode(ext2_inode_t* inode, char *name);
int ext2_dir_change_num(ext2_inode_t* inode, char *name,
			unsigned new_number);
struct inode_operations e2fs_inode_ops = {
	wrap_ext2_readfile,
	wrap_ext2_writefile,
	0,
	wrap_ext2_create,
	wrap_ext2_lookup,
	wrap_ext2_readdir,
	wrap_ext2_link,
	wrap_ext2_unlink,
	wrap_ext2_unlink,
	wrap_sync_inode,
	ext2_unmount,
	ext2_fs_stat,
	0,
	wrap_ext2_update
};

int ext2_fs_stat(struct inode *i, struct posix_statfs *f)
{
	if(!i || !f) return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	
	f->f_bsize = ext2_sb_blocksize(fs->sb);
	f->f_blocks = fs->sb->block_count;
	f->f_bfree = fs->sb->free_blocks;
	f->f_bavail = f->f_bfree;
	f->f_files = fs->sb->inode_count;
	f->f_ffree = fs->sb->free_inodes;
	f->f_type = 0xEF53;
	f->f_fsid = i->sb_idx;
	
	return 0;
}

struct inode *wrap_ext2_readdir(struct inode *in, unsigned num)
{
	num+=2;
	ext2_fs_t *fs = get_fs(in->sb_idx);
	if(!fs) return 0;
	ext2_inode_t inode;
	if (!ext2_inode_read(fs, in->num, &inode))
		return 0;
	char tmp[256];
	memset(tmp, 0, 256);
	int ret = ext2_dir_getnum(&inode, num, tmp);
	if(!ret) return 0;
	if(!ext2_inode_read(fs, ret, &inode))
		return 0;
	return create_sea_inode(&inode, tmp);
}

struct inode *wrap_ext2_lookup(struct inode *in, char *name)
{
	ext2_fs_t *fs = get_fs(in->sb_idx);
	if(!fs) return 0;
	ext2_inode_t inode;
	ext2_inode_read(fs, in->num, &inode);
	int ret = ext2_dir_get_inode(&inode, name);
	if(!ret) 
		return 0;
	ext2_inode_read(fs, ret, &inode);
	return create_sea_inode(&inode, name);
}

int wrap_ext2_update(struct inode *i)
{
	if(!i)
		return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, i->num, &inode))
		return -EIO;
	update_sea_inode(i, &inode, 0);
	return 0;
}

int wrap_ext2_readfile(struct inode *in, off_t off, size_t len, char *buf)
{
	ext2_fs_t *fs = get_fs(in->sb_idx);
	if(!fs)
		return -EINVAL;
	ext2_inode_t inode;
	size_t x = len;
	if(!ext2_inode_read(fs, in->num, &inode))
		return -EIO;
	if((unsigned)off >= inode.size)
		return 0;
	if(inode.deletion_time)
		return -ENOENT;
	if(!inode.mode)
		return -EACCES;
	if((off + len) >= (unsigned)in->len)
		len = in->len - off;
	unsigned int ret = ext2_inode_readdata(&inode, off, len, (unsigned char *)buf);
	return (int)ret;
}

int wrap_ext2_writefile(struct inode *in, off_t off, size_t len, char *buf)
{
	ext2_fs_t *fs = get_fs(in->sb_idx);
	if(!fs)
		return -EINVAL;
	if(fs->read_only)
		return -EROFS;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, in->num, &inode))
		return -EIO;
	if(inode.deletion_time || !inode.mode)
		return -ENOENT;
	unsigned sz = inode.size;
	unsigned sc = inode.sector_count;
	unsigned int ret = ext2_inode_writedata(&inode, off, len, (unsigned char*)buf);
	if(sz != inode.sector_count || sz != inode.size) 
		update_sea_inode(in, &inode, 0);
	if(ret > len) ret = len;
	return ret;
}

int do_add_ent(struct inode *i, ext2_inode_t *inode, char *name)
{
	if(!i || !name || !inode) return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	if(fs->read_only)
		return -EROFS;
	ext2_inode_t dir, told;
	if(!ext2_inode_read(fs, i->num, &dir))
		return -EIO;
	if(dir.deletion_time)
		return -ENOENT;
	if(!dir.mode)
		return -EACCES;
	if(!EXT2_INODE_IS_DIR(&dir))
		return -ENOTDIR;
	int ret = ext2_dir_link(&dir, inode, name);
	if(EXT2_INODE_IS_DIR(inode))
	{
		int old = ext2_dir_change_num(inode, "..", dir.number);
		mutex_acquire(&fs->fs_lock);
		dir.link_count++;
		ext2_inode_update(&dir);
		ext2_inode_read(fs, old, &told);
		told.link_count--;
		ext2_inode_update(&told);
		mutex_release(&fs->fs_lock);
	} else
		ext2_inode_update(&dir);
	update_sea_inode(i, &dir, 0);
	return 0;
}

int do_wrap_ext2_link(struct inode *i, char *path)
{
	if(!i || !path) return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	if(fs->read_only)
		return -EROFS;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, i->num, &inode))
		return -EIO;
	if(inode.deletion_time)
		return -ENOENT;
	if(!inode.mode)
		return -EACCES;
	char *p = strrchr(path, '/');
	struct inode *dir=0;
	/* Check if its in the same directory. If so, its easy */
	if(!p) {
		dir = get_idir(".", 0);
	} else {
		*p=0;
		dir = get_idir(path, 0);
	}
	/* Now, loc has the directory to link it into, and p contains 
	 * the name of the entry */
	if(!dir)
		return -ENOENT;
	/* Can't link accross multiple filesystems */
	if(dir->sb_idx != i->sb_idx)
		return -EINVAL;
	int ret = do_add_ent(dir, &inode, p ? p+1 : path);
	iput(dir);
	return ret;
}

int wrap_ext2_link(struct inode *i, char *path)
{
	int ret = do_wrap_ext2_link(i, path);
	wrap_ext2_update(i);
	return ret;
}

struct inode *do_wrap_ext2_create(struct inode *i, char *name, mode_t mode)
{
	if(!i)
		return 0;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return 0;
	if(fs->read_only) return 0;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, i->num, &inode))
		return 0;
	if(inode.deletion_time)
		return 0;
	if(!inode.mode)
		return 0;
	if(S_ISDIR(mode))
	{
		ext2_inode_t dir;
		int ret = ext2_dir_create(&inode, name, &dir);
		dir.mode = mode;
		dir.change_time = get_epoch_time();
		ext2_inode_update(&dir);
		update_sea_inode(i, &inode, 0);
		return create_sea_inode(&dir, name);
	}
	ext2_inode_t new;
	if (!ext2_inode_alloc(fs, &new))
		return 0;
	new.deletion_time = 0;
	new.mode = mode;
	new.uid = current_task->thread->uid;
	new.gid = current_task->thread->gid;
	if(!ext2_dir_link(&inode, &new, name)) 
		return 0;
	new.change_time = get_epoch_time();
	ext2_inode_update(&new);
	ext2_inode_update(&inode);
	update_sea_inode(i, &inode, 0);
	return create_sea_inode(&new, name);
}

struct inode *wrap_ext2_create(struct inode *i, char *name, unsigned mode)
{
	struct inode *ret = do_wrap_ext2_create(i, name, mode);
	return ret;
}

int do_wrap_ext2_unlink(struct inode *i)
{
	if(!i || !i->parent)
		return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	if(fs->read_only)
		return -EROFS;
	struct inode *test=0;
	test = S_ISDIR(i->mode) ? wrap_ext2_readdir(i, 0) : 0;
	if(test)
		return -ENOTEMPTY;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, i->parent->num, &inode))
		return -EIO;
	if(inode.deletion_time)
		return -ENOENT;
	if(!inode.mode)
		return -EACCES;
	int ret = ext2_dir_unlink(&inode, i->name, 1);
	update_sea_inode(i, &inode, 0);
	return ret-1;
}

int wrap_ext2_unlink(struct inode *i)
{
	int ret = do_wrap_ext2_unlink(i);
	return ret;
}

int ext2_inode_truncate(ext2_inode_t* inode, uint32_t size, int);
int wrap_sync_inode(struct inode *i)
{
	int new_type=-1;
	if(!i)
		return -EINVAL;
	ext2_fs_t *fs = get_fs(i->sb_idx);
	if(!fs) return -EINVAL;
	if(fs->read_only)
		return -EROFS;
	ext2_inode_t inode;
	if(!ext2_inode_read(fs, i->num, &inode))
		return -EIO;
	if(inode.link_count < 1 && !i->f_count) {
		//ext2_inode_free(&inode);
		//ext2_inode_update(&inode);
		//return 0;
	}
	if(inode.size > (unsigned)i->len)
		ext2_inode_truncate(&inode, i->len, 0);
	if((inode.mode & (~0xFFF)) != (i->mode & (~0xFFF)))
	{
		/* The filetype has changed. We must update the directory entry. */
		inode.mode = i->mode;
		new_type = ext2_inode_type(&inode);
	}
	copyto_ext2_inode(i, &inode);
	ext2_inode_update(&inode);
	if(new_type >= 0)
	{
		if(!i->parent)
		{
			printk(4, "[ext2]: Failed to discover parent node (i->name=%s)\n", i->name);
		} else {
			ext2_inode_t par;
			if(!ext2_inode_read(fs, i->parent->num, &par))
				return -EIO;
			ext2_dir_change_type(&par, i->name, new_type);
		}
	}
	return 0;
}

int update_sea_inode(struct inode *out, ext2_inode_t *in, char *name)
{
	if(!in) return 0;
	if(!out) return 0;
	out->mode = in->mode;
	out->uid = in->uid;
	out->len = in->size;
	out->atime = in->access_time;
	out->mtime = in->modification_time;
	out->ctime = in->change_time;
	out->gid = in->gid;
	out->nlink = in->link_count;
	out->num = in->number;
	out->sb_idx = in->fs->flag;
	out->nblocks = in->sector_count;
	out->dynamic=1;
	out->i_ops = &e2fs_inode_ops;
	if(name) strncpy(out->name, name, 64);
	return 1;
}

struct inode *create_sea_inode(ext2_inode_t *in, char *name)
{
	if(!in) return 0;
	struct inode *out = (struct inode *)kmalloc(sizeof(struct inode));
	if(!out)
		return 0;
	out->mode = in->mode;
	out->uid = in->uid;
	out->len = in->size;
	out->atime = in->access_time;
	out->mtime = in->modification_time;
	out->ctime = in->change_time;
	out->gid = in->gid;
	out->nlink = in->link_count;
	out->num = in->number;
	out->nblocks = in->sector_count;
	out->sb_idx = in->fs->flag;
	out->dynamic=1;
	out->flm = mutex_create(0, 0);
	out->i_ops = &e2fs_inode_ops;
	rwlock_create(&out->rwl);
	strncpy(out->name, name, 128);
	return out;
}

int copyto_ext2_inode(struct inode *out, ext2_inode_t *in)
{
	if(!in) return 0;
	if(!out) return 0;
	in->mode = out->mode;
	in->uid = out->uid;
	in->size = out->len;
	in->access_time = out->atime;
	in->modification_time = out->mtime;
	in->gid = out->gid;
	return 1;
}
