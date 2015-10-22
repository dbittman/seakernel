#include <stdint.h>
#include <stdbool.h>
#include <sea/types.h>
#include <sea/lib/linkedlist.h>
#include <sea/mutex.h>
#include <sea/kobj.h>
#include <sea/errno.h>
#include <sea/lib/hash.h>

#define __lock(h) do { if(!(h->flags & HASH_LOCKLESS)) mutex_acquire(&h->lock); } while(0)
#define __unlock(h) do { if(!(h->flags & HASH_LOCKLESS)) mutex_release(&h->lock); } while(0)

struct hash *hash_create(struct hash *h, int flags, size_t length)
{
	KOBJ_CREATE(h, flags, HASH_ALLOC);
	mutex_create(&h->lock, 0);
	h->table = kmalloc(length * sizeof(struct linkedlist *));
	h->length = length;
	return h;
}

void hash_destroy(struct hash *h)
{
	for(size_t index = 0;index < h->length;index++) {
		if(h->table[index])
			linkedlist_destroy(h->table[index]);
	}
	kfree(h->table);
	mutex_destroy(&h->lock);
	KOBJ_DESTROY(h, HASH_ALLOC);
}

static size_t __hashfn(const void *key, size_t keylen, size_t table_len)
{
	size_t hash = 5381;
	const unsigned char *buf = key;
	for(unsigned int i = 0;i < keylen;i++) {
		unsigned char e = buf[i];
		hash = ((hash << 5) + hash) + e;
	}
	return hash % table_len;
}

static bool __same_keys(const void *key1, size_t key1len, const void *key2, size_t key2len)
{
	if(key1len != key2len)
		return false;
	return memcmp(key1, key2, key1len) == 0 ? true : false;
}

static bool __ll_check_exist(struct linkedentry *ent, void *data)
{
	struct hashelem *he = data;
	struct hashelem *this = ent->obj;
	return __same_keys(he->key, he->keylen, this->key, this->keylen);
}

int hash_insert(struct hash *h, const void *key, size_t keylen, struct hashelem *elem, void *data)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	elem->ptr = data;
	elem->key = key;
	elem->keylen = keylen;
	if(h->table[index] == NULL) {
		/* lazy-init the buckets */
		h->table[index] = linkedlist_create(0, LINKEDLIST_LOCKLESS);
	} else {
		struct linkedentry *ent = linkedlist_find(h->table[index], __ll_check_exist, elem);
		if(ent) {
			__unlock(h);
			return -EEXIST;
		}
	}
	linkedlist_insert(h->table[index], &elem->entry, elem);
	h->count++;
	__unlock(h);
	return 0;
}

int hash_delete(struct hash *h, const void *key, size_t keylen)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	if(h->table[index] == NULL) {
		__unlock(h);
		return -ENOENT;
	}
	struct hashelem tmp;
	tmp.key = key;
	tmp.keylen = keylen;
	struct linkedentry *ent = linkedlist_find(h->table[index], __ll_check_exist, &tmp);
	if(ent) {
		linkedlist_remove(h->table[index], ent);
		h->count--;
	}
	__unlock(h);
	return ent ? 0 : -ENOENT;
}

void *hash_lookup(struct hash *h, const void *key, size_t keylen)
{
	__lock(h);
	size_t index = __hashfn(key, keylen, h->length);
	if(h->table[index] == NULL) {
		__unlock(h);
		return NULL;
	}
	struct hashelem tmp;
	tmp.key = key;
	tmp.keylen = keylen;
	struct linkedentry *ent = linkedlist_find(h->table[index], __ll_check_exist, &tmp);
	void *ret = NULL;
	if(ent) {
		struct hashelem *elem = ent->obj;
		ret = elem->ptr;
	}
	__unlock(h);
	return ret;
}

static void __fnjmp(struct linkedentry *ent, void *data)
{
	void (*fn)(struct hashelem *) = data;
	fn(ent->obj);
}

void hash_map(struct hash *h, void (*fn)(struct hashelem *obj))
{
	__lock(h);
	for(size_t index = 0;index < h->length;index++) {
		if(h->table[index])
			linkedlist_apply_data(h->table[index], __fnjmp, fn);
	}
	__unlock(h);
}

