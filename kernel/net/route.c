#include <sea/net/route.h>
#include <sea/net/interface.h>
#include <sea/ll.h>

static struct llist *table = 0;

/* this function does the actual routing algorithm. addr is the
 * destination address, and the function returns the route entry
 * for how to route it. */
struct route *net_route_get_entry(union ipv4_address addr)
{
	if(!table)
		return 0;
	return 0;
}

void net_route_add_entry(struct route *r)
{
	if(!table)
		table = ll_create(0);
}

void net_route_del_entry(struct route *r)
{
	assert(table);
}

