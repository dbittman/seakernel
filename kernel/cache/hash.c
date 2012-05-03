#include <kernel.h>
#include <cache.h>
#include <task.h>
static unsigned chach_get_location(chash_t *h, unsigned id, unsigned key)
{
	return ((id+key) * key) % h->length;
}

chash_t *chash_create(unsigned length)
{
	chash_t *h = (void *)kmalloc(sizeof(chash_t));
	h->length = length;
	h->hash = (chash_chain_t **)kmalloc(length * sizeof(chash_chain_t *));
	return h;
}

int chash_destroy(chash_t *h)
{
	kfree(h->hash);
	kfree(h);
	return 0;
}

chash_chain_t *do_chash_search(chash_t *h, unsigned id, unsigned key)
{
	unsigned i = chach_get_location(h, id, key);
	chash_chain_t *chain = h->hash[i];
	int q=0;
	while(chain) {
		q++;
		if(chain->key == key && chain->id == id)
			return chain;
		chain = chain->next;
	}
	return 0;
}

void *chash_search(chash_t *h, unsigned id, unsigned key)
{
	chash_chain_t *chain = do_chash_search(h, id, key);
	return chain ? chain->ptr : 0;
}

int chash_delete(chash_t *h, unsigned id, unsigned key)
{
	unsigned i = chach_get_location(h, id, key);
	chash_chain_t *el = do_chash_search(h, id, key);
	if(el->prev)
		el->prev->next = el->next;
	else
		h->hash[i] = el->next;
	if(el->next)
		el->next->prev = el->prev;
	kfree(el);
	return 0;
}

int chash_add(chash_t *h, unsigned id, unsigned key, void *ptr)
{
	chash_chain_t *new = (void*)kmalloc(sizeof(chash_chain_t));
	new->id=id;
	new->key=key;
	new->ptr=ptr;
	unsigned i = chach_get_location(h, id, key);
	chash_chain_t *chain = h->hash[i];
	new->next = chain;
	if(chain) chain->prev = new;
	h->hash[i] = new;
	return 0;
}
