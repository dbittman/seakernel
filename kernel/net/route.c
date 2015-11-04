#include <sea/net/route.h>
#include <sea/net/interface.h>
#include <sea/ll.h>
#include <sea/rwlock.h>
#include <sea/fs/proc.h>
#include <sea/vsprintf.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/string.h>
#include <sea/kernel.h>
#include <sea/trace.h>
#include <sea/fs/kerfs.h>
/* TODO: make thread-safe */
static struct linkedlist *table = 0;

/* TODO: generics */
#define NETWORK_PREFIX(addr,mask) (addr & mask)

/*
 * ref: http://graphics.stanford.edu/~seander/bithacks.html#CountBitsSetParallel
 */
unsigned int bit_count(uint32_t v)
{
	v = v - ((v >> 1) & 0x55555555);                    // reuse input as temporary
	v = (v & 0x33333333) + ((v >> 2) & 0x33333333);     // temp
	return ((v + ((v >> 4) & 0xF0F0F0F)) * 0x1010101) >> 24; // count
}

/* confidence of -1 means DO NOT send the packet that way. Confidence of
 * any positive number or zero means valid route, with the highest confidence
 * value being the best choice (and the default route being zero confidence)
 */
int __net_route_calc_confidence(struct route *r, uint32_t addr)
{
	if(!(r->flags & ROUTE_FLAG_UP))
			return -1;
	if(!(r->interface->flags & IFACE_FLAG_UP))
		return -1;
	uint32_t prefix = NETWORK_PREFIX(addr, r->netmask);
	if(prefix == r->destination) {
		return bit_count(r->netmask);
	}
	/* default route has a confidence of 0, but is still a possible route */
	if(r->flags & ROUTE_FLAG_DEFAULT)
		return 0;
	return -1;
}

/* this function does the actual routing algorithm. addr is the
 * destination address, and the function returns the route entry
 * for how to route it. */
struct route *net_route_select_entry(uint32_t addr)
{
	if(!table)
		return 0;
	struct route *r, *best = 0;
	struct linkedentry *node;
	int i = 0, max_score = -1;
	
	__linkedlist_lock(table);
	for(node = linkedlist_iter_start(table);
			node != linkedlist_iter_end(table);
			node = linkedlist_iter_next(node)) {
		r = linkedentry_obj(node);
		int confidence = __net_route_calc_confidence(r, addr);
		if(confidence >= 0) {
			/* possible route! */
			if(!best || max_score < confidence) {
				max_score = confidence;
				best = r;
			}
		}
	}
	__linkedlist_unlock(table);
	return best;
}

void net_route_add_entry(struct route *r)
{
	if(!table)
		table = linkedlist_create(0, LINKEDLIST_MUTEX);
	linkedlist_insert(table, &r->node, r);
}

void net_route_del_entry(struct route *r)
{
	assert(table);
	linkedlist_remove(table, &r->node);
}

int net_route_find_del_entry(uint32_t dest, struct net_dev *nd)
{
	struct route *r, *del=0;
	struct linkedentry *node;
	__linkedlist_lock(table);
	for(node = linkedlist_iter_start(table);
			node != linkedlist_iter_end(table);
			node = linkedlist_iter_next(node)) {
		r = linkedentry_obj(node);
		if(r->destination == dest && r->interface == nd) {
			del = r;
		}
	}
	__linkedlist_unlock(table);
	if(del) {
		linkedlist_remove(table, &del->node);
		kfree(del);
	}
	return del ? 0 : -ENOENT;
}

static void write_addr(char *str, uint32_t addr)
{
	snprintf(str, 32, "%d.%d.%d.%d", (addr) & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF);
}

int kerfs_route_report(int direction, void *param, size_t size, size_t offset, size_t length, char *buf)
{
	size_t current = 0;
	KERFS_PRINTF(offset, length, buf, current,
			"DEST            GATEWAY         MASK            FLAGS IFACE\n");
	if(!table)
		return current;
	struct route *r, *del=0;
	struct linkedentry *node;
	__linkedlist_lock(table);
	for(node = linkedlist_iter_start(table);
			node != linkedlist_iter_end(table);
			node = linkedlist_iter_next(node)) {
		r = linkedentry_obj(node);
		char dest[32];
		char gate[32];
		char mask[32];
		write_addr(dest, r->destination);
		write_addr(gate, r->gateway);
		write_addr(mask, r->netmask);
		KERFS_PRINTF(offset, length, buf, current,
				"%s \t%s \t%s \t", dest, gate, mask);

		KERFS_PRINTF(offset, length, buf, current,
				"%c%c%c%c  %s\n",
				(r->flags & ROUTE_FLAG_UP) ? 'U' : ' ',
				(r->flags & ROUTE_FLAG_HOST) ? 'H' : ' ',
				(r->flags & ROUTE_FLAG_GATEWAY) ? 'G' : ' ',
				(r->flags & ROUTE_FLAG_DEFAULT) ? 'D' : ' ',
				r->interface->name);

	}
	__linkedlist_unlock(table);

	return current;
}

