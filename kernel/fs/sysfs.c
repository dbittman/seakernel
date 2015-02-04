#include <sea/fs/inode.h>
#include <sea/types.h>
#include <sea/lib/hash.h>
#include <sea/cpu/atomic.h>
#include <sea/errno.h>
#include <sea/fs/kerfs.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
static dev_t dev_num = 0;

static struct hash_table *table = 0;

struct kerfs_node {
	dev_t num;
	void *param;
	size_t size;
	int flags, type;
	int (*fn)(size_t, size_t, char *);
};

int kerfs_register_parameter(char *path, void *param, size_t size, int flags, int type)
{
	dev_t num = add_atomic(&dev_num, 1);
	int r = sys_mknod(path, S_IFREG | 0600, num);
	if(r < 0)
		return r;
	struct kerfs_node *kn = kmalloc(sizeof(struct kerfs_node));
	kn->num = num;
	kn->param = param;
	kn->size = size;
	kn->flags = flags | KERFS_PARAM;
	kn->type = type;

	assert(!hash_table_set_entry(table, &num, sizeof(num), 1, kn));
	return 0;
}

int kerfs_register_report(char *path, int (*fn)(size_t, size_t, char *))
{
	dev_t num = add_atomic(&dev_num, 1);
	int r = sys_mknod(path, S_IFREG | 0600, num);
	if(r < 0)
		return r;
	struct kerfs_node *kn = kmalloc(sizeof(struct kerfs_node));
	kn->num = num;
	kn->fn = fn;

	assert(!hash_table_set_entry(table, &num, sizeof(num), 1, kn));
	return 0;
}

int kerfs_read(struct inode *node, size_t offset, size_t length, char *buffer)
{
	struct kerfs_node *kn;
	if(hash_table_get_entry(table, &node->phys_dev, sizeof(node->phys_dev), 1, (void **)&kn) < 0)
		return -ENOENT;

	if(!(kn->flags & KERFS_PARAM)) {
		return kn->fn(offset, length, buffer);
	}

	char tmp[128];
	if(kn->type == KERFS_TYPE_INTEGER) {
		switch(kn->size) {
			case 1:
				snprintf(tmp, 128, "%d", *(uint8_t *)kn->param);
				break;
			case 2:
				snprintf(tmp, 128, "%d", *(uint16_t *)kn->param);
				break;
			case 4:
				snprintf(tmp, 128, "%d", *(uint32_t *)kn->param);
				break;
			case 8:
				snprintf(tmp, 128, "%d", *(uint64_t *)kn->param);
				break;
		}
	}
	
	if(offset > strlen(tmp))
		return 0;
	if(offset + length > strlen(tmp))
		length = strlen(tmp) - offset;

	memcpy(buffer, tmp + offset, length);
	return length;
}

int kerfs_write(struct inode *node, size_t offset, size_t length, char *buffer)
{
	struct kerfs_node *kn;
	if(hash_table_get_entry(table, &node->phys_dev, sizeof(node->phys_dev), 1, (void **)&kn) < 0)
		return -ENOENT;

	if(!(kn->flags & KERFS_PARAM) || offset > 0) {
		return -EIO;
	}

	char tmp[128];
	memset(tmp, 0, 128);
	if(length > 127)
		length = 127;
	memcpy(tmp, buffer, length);
	char *nl = strchr(tmp, '\n');
	if(nl) *nl=0;
	if(kn->type == KERFS_TYPE_INTEGER) {
		switch(kn->size) {
			case 1:
				*(uint8_t *)kn->param = (uint8_t)strtoint(tmp);
				break;
			case 2:
				*(uint16_t *)kn->param = (uint16_t)strtoint(tmp);
				break;
			case 4:
				*(uint32_t *)kn->param = (uint32_t)strtoint(tmp);
				break;
			case 8:
				*(uint64_t *)kn->param = (uint64_t)strtoint(tmp);
				break;
		}
	}
	return length;
}


void kerfs_init()
{
	table = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(table, HASH_RESIZE_MODE_IGNORE,1000);
	hash_table_specify_function(table, HASH_FUNCTION_BYTE_SUM);
}

