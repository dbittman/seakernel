#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/string.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#warning "todo: try to use reader locks"
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen)
{
	if(!vfs_inode_check_permissions(node, MAY_EXEC))
		return 0;
	rwlock_acquire(&node->lock, RWL_WRITER);
	struct dirent *dir = vfs_inode_get_dirent(node, name, namelen);
	if(!dir) {
		dir = vfs_dirent_create(node);
		dir->count = 1;
		int r = fs_callback_inode_get_dirent(node, name, namelen, dir);
		if(r) {
			vfs_dirent_destroy(dir);
			rwlock_release(&node->lock, RWL_WRITER);
			return 0;
		}
		vfs_inode_get(node);
		vfs_inode_add_dirent(node, name, namelen, dir);
	} else {
		if(add_atomic(&dir->count, 1) == 1)
			vfs_inode_get(node);
	}
	rwlock_release(&node->lock, RWL_WRITER);
}

#warning "locking?"
int fs_inode_pull(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_NEEDREAD) {
		r = fs_callback_inode_pull(node);
		if(!r)
			and_atomic(&node->flags, ~INODE_NEEDREAD);
		return r;
	}
}

int fs_inode_push(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_DIRTY) {
		r = fs_callback_inode_push(node);
		if(!r)
			and_atomic(&node->flags, ~INODE_DIRTY);
		return r;
	}
}

struct inode *fs_dirent_readinode(struct dirent *dir)
{
	struct inode *node = vfs_icache_get(dir->filesystem, dir->ino);
	if(fs_inode_pull(node))
		return 0;
	return node;
}

struct dirent *fs_resolve_path(const char *path, int flags)
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
	assert(start);
	if(!*path)
		path = ".";
	
	struct inode *node = start;
	struct dirent *dir = 0;
	while(node) {
		struct inode *nextnode = 0;
		char *delim = strchr(path, '/');
		if(delim != path) {
			const char *name = path;
			size_t namelen = delim ? delim - name : strlen(name);
			dir = fs_dirent_lookup(node, name, namelen);
			if(delim) {
				nextnode = fs_dirent_readinode(dir);
				vfs_dirent_release(dir);
			}
			node = nextnode;
		}
		path = delim + 1;
	}
	
	return dir;
}

struct inode *fs_resolve_path_inode(const char *path, int flags, int *error)
{
	struct dirent *dir = fs_resolve_path(path, flags);
	if(!dir) {
		if(error)
			*error = -ENOENT;
		return 0;
	}
	struct inode *node = fs_dirent_readinode(dir);
	vfs_dirent_release(dir);
	if(!node && error)
		*error = -EIO;
	return node;
}

struct inode *fs_resolve_path_create(const char *path, int flags, mode_t mode, int *did_create)
{
	size_t len = strlen(path);
	char tmp[len];
	strncpy(tmp, path, len);
	char *name = strrchr(tmp, '/');
	if(name) {
		*name = 0;
		name++;
	}
	struct inode *dir = fs_resolve_path_inode(tmp, flags, 0);
	if(!dir)
		return -ENOENT;

	struct dirent *test = fs_dirent_lookup(dir, name, strlen(name));
	if(test) {
		struct inode *ret = fs_dirent_readinode(test);
		vfs_dirent_release(test);
		if(did_create)
			*did_create = 0;
		return ret;
	}

	uint32_t id;
	int r = fs_callback_fs_alloc_inode(dir->filesystem, &id);
	if(r) {
		vfs_icache_put(dir);
		return -EIO;
	}

	struct inode *node = vfs_icache_get(dir->filesystem, id);
	fs_inode_pull(node);

	fs_link(dir, node, name, strlen(name));

	vfs_icache_put(dir);
	if(did_create)
		*did_create = 1;
	return node;
}

struct dirent *fs_readdir(struct inode *node, size_t num)
{
#warning "this is crap"
	return fs_callback_inode_readdir(node, num);
}

