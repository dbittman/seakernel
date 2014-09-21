#include <sea/net/route.h>
#include <sea/net/interface.h>
#include <sea/ll.h>
#include <sea/rwlock.h>
/* TODO: make thread-safe */
static struct llist *table = 0;

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
int __net_route_calc_confidence(struct route *r, union ipv4_address addr)
{
	if(!(r->flags & ROUTE_FLAG_UP))
			return -1;
	if(!(r->interface->flags & IFACE_FLAG_UP))
		return -1;
	uint32_t prefix = NETWORK_PREFIX(addr.address, r->netmask);
	if(prefix == r->destination.address) {
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
struct route *net_route_select_entry(union ipv4_address addr)
{
	if(!table)
		return 0;
	struct route *r, *best = 0;
	struct llistnode *node;
	int i = 0, max_score = -1;
	
	rwlock_acquire(&table->rwl, RWL_READER);
	ll_for_each_entry(table, node, struct route *, r) {
		int confidence = __net_route_calc_confidence(r, addr);
		if(confidence >= 0) {
			/* possible route! */
			if(!best || max_score < confidence) {
				max_score = confidence;
				best = r;
			}
		}
	}
	rwlock_release(&table->rwl, RWL_READER);
	return best;
}

void net_route_add_entry(struct route *r)
{
	if(!table)
		table = ll_create(0);
	r->node = ll_insert(table, r);
}

void net_route_del_entry(struct route *r)
{
	assert(table);
	ll_remove(table, r->node);
	r->node = 0;
}

