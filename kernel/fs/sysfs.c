#include <sea/kernel.h>
#include <sea/fs/inode.h>
#include <sea/types.h>
#include <sea/lib/hash.h>
#include <stdatomic.h>
#include <sea/errno.h>
#include <sea/fs/kerfs.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
static int minor = 0;
static int major;
static struct hash *table = 0;

struct kerfs_node {
	int num;
	void *param;
	size_t size;
	int flags;
	int (*fn)(int direction, void *, size_t, size_t, size_t, unsigned char *);
	struct hashelem hash_elem;
};

int kerfs_register_parameter(char *path, void *param, size_t size, int flags, int (*call)(int, void *, size_t, size_t, size_t, unsigned char *))
{
	uid_t old = current_process->effective_uid;
	current_process->effective_uid = 0;
	dev_t num = atomic_fetch_add(&minor, 1) + 1;
	int r = sys_mknod(path, S_IFREG | 0600, GETDEV(major, num));
	current_process->effective_uid = old;
	if(r < 0)
		return r;
	struct kerfs_node *kn = kmalloc(sizeof(struct kerfs_node));
	kn->num = num;
	kn->param = param;
	kn->size = size;
	kn->flags = flags;
	kn->fn = call;
	hash_insert(table, &kn->num, sizeof(kn->num), &kn->hash_elem, kn);
	return 0;
}

int kerfs_unregister_entry(char *path)
{
	uid_t old = current_process->effective_uid;
	current_process->effective_uid = 0;
	int err;
	struct inode *node = fs_path_resolve_inode(path, RESOLVE_NOLINK, &err);
	if(!node) {
		current_process->effective_uid = old;
		return err;
	}
	dev_t num = node->phys_dev;
	vfs_icache_put(node);

	hash_delete(table, &num, sizeof(num));
	sys_unlink(path);
	
	current_process->effective_uid = old;
	return 0;
}

int kerfs_rw_string(int direction, void *param, size_t sz,
		size_t offset, size_t length, unsigned char *buf)
{
	size_t current = 0;
	if(direction == READ) {
		KERFS_PRINTF(offset, length, buf, current,
				"%s", (char *)param);
	}
	return current;
}

int kerfs_rw_address(int direction, void *param, size_t sz,
		size_t offset, size_t length, unsigned char *buf)
{
	size_t current = 0;
	if(direction == READ) {
		KERFS_PRINTF(offset, length, buf, current,
				"%x", *(addr_t *)param);
	}
	return current;
}

int kerfs_rw_integer(int direction, void *param, size_t sz, size_t offset, size_t length,
		unsigned char *buf)
{
	size_t current = 0;
	uint64_t val = 0;
	memcpy(&val, param, sz);
	if(direction == READ) {
		KERFS_PRINTF(offset, length, buf, current,
				"%d", val);
	} else {
		val = strtoint((char *)buf);
		switch(sz) {
			case 1:
				*(uint8_t *)param = sz;
				break;
			case 2:
				*(uint16_t *)param = sz;
				break;
			case 4:
				*(uint32_t *)param = sz;
				break;
			case 8:
				*(uint64_t *)param = sz;
				break;
		}
		current = sz;
	}
	return current;
}

static ssize_t kerfs_read(struct inode *node, size_t offset, size_t length, unsigned char *buffer)
{
	struct kerfs_node *kn;
	int min = MINOR(node->phys_dev);
	if((kn = hash_lookup(table, &min, sizeof(min))) == NULL)
		return -ENOENT;

	return kn->fn(READ, kn->param, kn->size, offset, length, buffer);
}

static ssize_t kerfs_write(struct inode *node, size_t offset, size_t length, unsigned char *buffer)
{
	struct kerfs_node *kn;
	int min = MINOR(node->phys_dev);
	if((kn = hash_lookup(table, &min, sizeof(min))) == NULL)
		return -ENOENT;

	if(!(kn->flags & KERFS_PARAM_WRITE))
		return -EIO;

	return kn->fn(WRITE, kn->param, kn->size, offset, length, buffer);
}

static ssize_t kerfs_rw(int rw, struct file *file, off_t off, uint8_t *buffer, size_t len)
{
	if(rw == READ)
		return kerfs_read(file->inode, off, len, buffer);
	else if(rw == WRITE)
		return kerfs_write(file->inode, off, len, buffer);
	return -EIO;
}

static struct kdevice __kerfs_kdev = {
	.select = 0, .open = 0, .close = 0, .create = 0, .destroy = 0, .ioctl = 0,
	.name = "kerfs",
	.rw = kerfs_rw,
};

void kerfs_init(void)
{
	table = hash_create(0, 0, 1000);
	major = dm_device_register(&__kerfs_kdev);
}

