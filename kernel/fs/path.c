#include <sea/kernel.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/string.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#include <sea/fs/dir.h>

/* translate a mount point to the root node of the mounted filesystem */
struct inode *fs_resolve_mount(struct inode *node)
{
	struct inode *ret = node;
	if(node && node->mount) {
		ret = fs_read_root_inode(node->mount);
		vfs_icache_put(node);
	}
	return ret;
}

/* this is the main workhorse for the virtual filesystem. Given a start point and
 * a path string, it resolves the path into a directory entry. This is simply a
 * matter of looping through the path (seperated at '/', of course), and reading
 * the successive directory entries and inodes (see vfs_dirent_lookup and
 * vfs_dirent_readinode). The one thing it needs to pay attention to is the case
 * of traversing backwards up through a mount point. */
struct dirent *fs_do_path_resolve(struct inode *start, const char *path, int *result)
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

/* most of these function are convenience functions for calling do_fs_path_resolve. This one,
 * for example, is the default, get-the-dirent-using-normal-unix-path-rules ('/' starts at
 * root, etc) */
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
	return fs_do_path_resolve(start, path, result);
}

/* this one does the extra step of getting the inode pointed to by the dirent.
 * Since most fs functions just want this inode and don't care about the directory
 * entry that was used to point to it, this is used quite frequently. */
struct inode *fs_path_resolve_inode(const char *path, int flags, int *error)
{
	struct dirent *dir = fs_path_resolve(path, 0, error);
	if(!dir)
		return 0;
	struct inode *node = fs_dirent_readinode(dir, (flags & RESOLVE_NOLINK));
	if(!(flags & RESOLVE_NOMOUNT))
		node = fs_resolve_mount(node);
	vfs_dirent_release(dir);
	if(!node && error)
		*error = -EIO;
	return node;
}

/* this is possibly the lengthiest function in the VFS (possibly in the entire kernel!).
 * It resolves a path until the last name in the path string, and then tries to create
 * a new file with that name. It requires a lot of in-sequence checks for permission,
 * existance, is-a-directory, and so forth. */
struct inode *fs_path_resolve_create_get(const char *path,
		int flags, mode_t mode, int *result, struct dirent **dirent)
{
	/* step 1: split the path up into directory to create in, and name of new file. */
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

	/* step 2: resolve the target directory */
	struct inode *dir = fs_path_resolve_inode(dirpath, flags, result);
	if(!dir)
		return 0;
	if(!S_ISDIR(dir->mode)) {
		if(result) *result = -ENOTDIR;
		return 0;
	}

	/* step 3: try to look up the file that we're trying to create */
	struct dirent *test = fs_dirent_lookup(dir, name, strlen(name));
	if(test) {
		/* if it was found, return it and its inode. */
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
	
	/* didn't find the entry. Step 4: Create one */
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
	/* step 4a: allocate an inode */
	int r = fs_callback_fs_alloc_inode(dir->filesystem, &id);
	if(r) {
		if(result) *result = r;
		vfs_icache_put(dir);
		return 0;
	}

	/* step 4b: read in that inode, and set some initial values (like creation time) */
	struct inode *node = vfs_icache_get(dir->filesystem, id);
	node->mode = mode;
	node->length = 0;
	node->ctime = node->mtime = time_get_epoch();
	vfs_inode_set_dirty(node);
	
	/* if we're making a directory, create the . and .. entries */
	if(S_ISDIR(mode)) {
		/* create . and .. */
		if(fs_link(node, node, ".", 1))
			r = -EPERM;
		if(fs_link(node, dir, "..", 2))
			r = -EMLINK;
	}

	/* step 4c: create the link for the directory entry to the inode */
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
	return fs_path_resolve_create_get(path, flags, mode, result, 0);
}

