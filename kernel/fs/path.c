#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/string.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#include <sea/fs/dir.h>
struct inode *fs_resolve_mount(struct inode *node)
{
	struct inode *ret = node;
	if(node && node->mount) {
		ret = fs_read_root_inode(node->mount);
		vfs_icache_put(node);
	}
	return ret;
}

struct dirent *do_fs_path_resolve(struct inode *start, const char *path, int *result)
{
	vfs_inode_get(start);
	assert(start);
	if(!*path)
		path = ".";
	if(result) *result = 0;
	
	struct inode *node = start;
	struct dirent *dir = 0;
	while(node && *path) {
		if(dir)
			vfs_dirent_release(dir);
		struct inode *nextnode = 0;
		char *delim = strchr(path, '/');
		if(delim != path) {
			const char *name = path;
			size_t namelen = delim ? (size_t)(delim - name) : strlen(name);
			dir = fs_dirent_lookup(node, name, namelen);
			if(!dir) {
				if(result) *result = -ENOENT;
				return 0;
			}
			if(delim) {
				nextnode = fs_dirent_readinode(dir, 0);
				if(namelen == 2 && !strncmp("..", name, 2)
						&& node->id == node->filesystem->root_inode_id) {
					/* traverse back up through a mount */
					vfs_inode_get(nextnode->filesystem->point);
					struct inode *tmp = nextnode;
					nextnode = nextnode->filesystem->point;
					vfs_icache_put(tmp);
				} else {
					nextnode = fs_resolve_mount(nextnode);
				}
			}
			vfs_icache_put(node);
			node = nextnode;
		}
		path = delim + 1;
	}
	
	return dir;
}

struct dirent *fs_path_resolve(const char *path, int flags, int *result)
{
	struct inode *start;
	if(!path)
		path = ".";
	if(path[0] == '/') {
		start = current_task->thread->root;
		path++;
	} else {
		start = current_task->thread->pwd;
	}
	return do_fs_path_resolve(start, path, result);
}

struct inode *fs_path_resolve_inode(const char *path, int flags, int *error)
{
	struct dirent *dir = fs_path_resolve(path, 0, error);
	if(!dir)
		return 0;
	struct inode *node = fs_dirent_readinode(dir, (flags & RESOLVE_NOLINK));
	node = fs_resolve_mount(node);
	vfs_dirent_release(dir);
	if(!node && error)
		*error = -EIO;
	return node;
}

struct inode *do_fs_path_resolve_create(const char *path,
		int flags, mode_t mode, int *result, struct dirent **dirent)
{
	int len = strlen(path) + 1;
	char tmp[len];
	memcpy(tmp, path, len);
	char *del = strrchr(tmp, '/');
	if(del) *del = 0;
	char *dirpath = del ? tmp : ".";
	char *name = del ? del + 1 : tmp;
	if(dirpath[0] == 0)
		dirpath = "/";
	if(dirent) *dirent=0;
	if(result) *result=0;

	struct inode *dir = fs_path_resolve_inode(dirpath, flags, result);
	if(!dir)
		return 0;
	if(!S_ISDIR(dir->mode)) {
		if(result) *result = -ENOTDIR;
		return 0;
	}

	struct dirent *test = fs_dirent_lookup(dir, name, strlen(name));
	if(test) {
		if(dirent)
			*dirent = test;
		struct inode *ret = fs_dirent_readinode(test, flags & RESOLVE_NOLINK);
		if(!ret) {
			if(dirent)
				*dirent = 0;
			if(result)
				*result = -EIO;
			vfs_dirent_release(test);
		} else {
			ret = fs_resolve_mount(ret);
			if(!dirent)
				vfs_dirent_release(test);
			if(result)
				*result = 0;
		}
		vfs_icache_put(dir);
		return ret;
	}
	
	/* didn't find the entry. Create one */
	if(!vfs_inode_check_permissions(dir, MAY_WRITE, 0)) {
		if(result) *result = -EACCES;
		vfs_icache_put(dir);
		return 0;
	}
	if(dir->nlink == 1) {
		if(result) *result = -ENOSPC;
		vfs_icache_put(dir);
		return 0;
	}

	uint32_t id;
	int r = fs_callback_fs_alloc_inode(dir->filesystem, &id);
	if(r) {
		if(result) *result = r;
		vfs_icache_put(dir);
		return 0;
	}

	struct inode *node = vfs_icache_get(dir->filesystem, id);
	node->mode = mode;
	node->length = 0;
	node->ctime = node->mtime = time_get_epoch();
	vfs_inode_set_dirty(node);
	
	if(S_ISDIR(mode)) {
		/* create . and .. */
		if(fs_link(node, node, ".", 1))
			r = -EPERM;
		if(fs_link(node, dir, "..", 2))
			r = -EMLINK;
	}

	r = fs_link(dir, node, name, strlen(name));

	if(result)
		*result = !r ? 1 : r;
	if(dirent && !r) {
		*dirent = fs_dirent_lookup(dir, name, strlen(name));
		if(!*dirent && node) {
			vfs_icache_put(node);
			vfs_icache_put(dir);
			if(result)
				*result = -EIO;
			return 0;
		}
	}
	vfs_icache_put(dir);
	return r ? 0 : node;
}

struct inode *fs_path_resolve_create(const char *path,
		int flags, mode_t mode, int *result)
{
	return do_fs_path_resolve_create(path, flags, mode, result, 0);
}

