#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/string.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#warning "todo: try to use reader locks"
static struct dirent *do_fs_resolve_path(struct inode *start, const char *path, int flags);
struct dirent *fs_dirent_lookup(struct inode *node, const char *name, size_t namelen)
{
	if(!vfs_inode_check_permissions(node, MAY_EXEC, 0))
		return 0;
	if(node == current_task->thread->root && !strncmp(name, "..", 2) && namelen == 2)
		return fs_dirent_lookup(node, ".", 1);
	rwlock_acquire(&node->lock, RWL_WRITER);
	struct dirent *dir = vfs_inode_get_dirent(node, name, namelen);
	if(!dir) {
		dir = vfs_dirent_create(node);
		dir->count = 1;
		strncpy(dir->name, name, namelen);
		dir->namelen = namelen;
		int r = fs_callback_inode_lookup(node, name, namelen, dir);
		if(r) {
			dir->count = 0;
			vfs_dirent_destroy(dir);
			rwlock_release(&node->lock, RWL_WRITER);
			return 0;
		}
		vfs_inode_get(node);
		vfs_inode_add_dirent(node, dir);
	} else {
		if(add_atomic(&dir->count, 1) == 1)
			vfs_inode_get(node);
	}
	rwlock_release(&node->lock, RWL_WRITER);
	return dir;
}

#warning "locking?"
int fs_inode_pull(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_NEEDREAD) {
		r = fs_callback_inode_pull(node);
		if(!r)
			and_atomic(&node->flags, ~INODE_NEEDREAD);
	}
	return r;
}

int fs_inode_push(struct inode *node)
{
	int r = 0;
	if(node->flags & INODE_DIRTY) {
		r = fs_callback_inode_push(node);
		if(!r)
			and_atomic(&node->flags, ~INODE_DIRTY);
	}
	return r;
}

struct inode *fs_dirent_readinode(struct dirent *dir, int nofollow)
{
	assert(dir);
	struct inode *node = vfs_icache_get(dir->filesystem, dir->ino);
	assert(node);
	if(fs_inode_pull(node))
		return 0;
	if(!nofollow && S_ISLNK(node->mode)) {
		char link[node->length+1]; //TODO: fix possible DoS attack
		if((size_t)fs_inode_read(node, 0, node->length, link) != node->length)
			return 0;
		link[node->length]=0;
		char *newpath = link;
		struct inode *start = dir->parent, *ln=0;
		if(link[0] == '/') {
			newpath++;
			start = current_task->thread->root;
		}
		struct dirent *ln_dir = do_fs_resolve_path(start, link, 0);
		vfs_icache_put(node);
		if(ln_dir) {
			ln = fs_dirent_readinode(ln_dir, 0);
			vfs_dirent_release(ln_dir);
		}


		node = ln;
	}
	return node;
}

struct inode *fs_resolve_mount(struct inode *node)
{
	struct inode *ret = node;
	if(node && node->mount) {
		ret = fs_read_root_inode(node->mount);
		vfs_icache_put(node);
	}
	return ret;
}

static struct dirent *do_fs_resolve_path(struct inode *start, const char *path, int flags)
{	
	vfs_inode_get(start);
	assert(start);
	if(!*path)
		path = ".";
	
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
				return 0;
			}
			if(delim) {
				nextnode = fs_dirent_readinode(dir, (flags & RESOLVE_NOLINK));
#warning "should we do this in more places?"
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
	return do_fs_resolve_path(start, path, flags);
}

struct inode *fs_resolve_path_inode(const char *path, int flags, int *error)
{
	struct dirent *dir = fs_resolve_path(path, flags);
	if(!dir) {
		if(error)
			*error = -ENOENT;
		return 0;
	}
	struct inode *node = fs_dirent_readinode(dir, (flags & RESOLVE_NOLINK));
	node = fs_resolve_mount(node);
	vfs_dirent_release(dir);
	if(!node && error)
		*error = -EIO;
	return node;
}

struct inode *fs_resolve_path_create(const char *path, int flags, mode_t mode, int *did_create)
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

	struct inode *dir = fs_resolve_path_inode(dirpath, flags, 0);
	if(!dir)
		return 0;

	struct dirent *test = fs_dirent_lookup(dir, name, strlen(name));
	if(test) {
		struct inode *ret = fs_dirent_readinode(test, flags & RESOLVE_NOLINK);
		ret = fs_resolve_mount(ret);
		vfs_dirent_release(test);
		if(did_create)
			*did_create = 0;
		return ret;
	}

	uint32_t id;
	int r = fs_callback_fs_alloc_inode(dir->filesystem, &id);
	if(r) {
		vfs_icache_put(dir);
		return 0;
	}

	struct inode *node = vfs_icache_get(dir->filesystem, id);
	fs_inode_pull(node);
	node->mode = mode;
	vfs_inode_set_dirty(node);
#warning "find all the times to do this..."
	fs_inode_push(node);

	r = fs_link(dir, node, name, strlen(name));

	if(S_ISDIR(mode)) {
		/* create . and .. */
		if(fs_link(node, node, ".", 1))
			r = -EPERM;
		if(fs_link(node, dir, "..", 2))
			r = -EMLINK;
	}

	vfs_icache_put(dir);
	if(did_create)
		*did_create = 1;
	return r == 0 ? node : 0;
}

