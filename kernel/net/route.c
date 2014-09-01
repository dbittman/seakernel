#include <sea/net/route.h>
#include <sea/net/interface.h>
#include <sea/ll.h>
#include <sea/rwlock.h>
/* TODO: make thread-safe */
static struct llist *table = 0;

/* confidence of -1 means DO NOT send the packet that way. Confidence of
 * any positive number or zero means valid route, with the highest confidence
 * value being the best choice (and the default route being zero confidence)
 */
int __net_route_calc_confidence(struct route *r, union ipv4_address addr)
{
	uint32_t prefix = NETWORK_PREFIX(addr.address, r->netmask);
	if(prefix == r->destination.address) {
		/* this route works! our 'confidence' is the number of bits
		 * that match in the netmask and the address. Since we only
		 * care if the network prefix is equal to the route destination, 
		 * that just means the number of bits set in the netmask. Because
		 * of the way netmasks are formatted, the more bits are set, the
		 * larger the number, so we can just that as the confidence. */
		return r->netmask;
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

