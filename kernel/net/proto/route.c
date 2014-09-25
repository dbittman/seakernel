#include <sea/net/route.h>
#include <sea/net/interface.h>
#include <sea/ll.h>
#include <sea/rwlock.h>
#include <sea/fs/proc.h>
#include <sea/vsprintf.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
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

int net_route_find_del_entry(uint32_t dest, struct net_dev *nd)
{
	rwlock_acquire(&table->rwl, RWL_WRITER);
	struct route *r, *del=0;
	struct llistnode *node;
	ll_for_each_entry(table, node, struct route *, r) {
		if(r->destination.address == dest && r->interface == nd) {
			del = r;
		}
	}
	rwlock_release(&table->rwl, RWL_WRITER);
	if(del) {
		ll_remove(table, del->node);
		del->node = 0;
		kfree(del);
	}
	return del ? 0 : -ENOENT;
}

static void write_addr(char *str, uint32_t addr)
{
	snprintf(str, 32, "%d.%d.%d.%d", (addr) & 0xFF, (addr >> 8) & 0xFF, (addr >> 16) & 0xFF, (addr >> 24) & 0xFF);
}

int proc_read_route(char *buf, int off, int len)
{
	int i, total_len=0;
	total_len += proc_append_buffer(buf, "DEST            GATEWAY         MASK            FLAGS IFACE\n", total_len, -1, off, len);
	if(!table)
		return total_len;
	rwlock_acquire(&table->rwl, RWL_READER);
	struct llistnode *node;
	struct route *r;
	ll_for_each_entry(table, node, struct route *, r) {
		char tmp[32];
		memset(tmp, 0, 32);
		write_addr(tmp, r->destination.address);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, " \t", total_len, -1, off, len);
		
		write_addr(tmp, r->gateway.address);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, " \t", total_len, -1, off, len);
		
		write_addr(tmp, r->netmask);
		total_len += proc_append_buffer(buf, tmp, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, " \t", total_len, -1, off, len);

		if(r->flags & ROUTE_FLAG_UP)
			total_len += proc_append_buffer(buf, "U", total_len, -1, off, len);
		else
			total_len += proc_append_buffer(buf, " ", total_len, -1, off, len);

		if(r->flags & ROUTE_FLAG_HOST)
			total_len += proc_append_buffer(buf, "H", total_len, -1, off, len);
		else
			total_len += proc_append_buffer(buf, " ", total_len, -1, off, len);
		if(r->flags & ROUTE_FLAG_GATEWAY)
			total_len += proc_append_buffer(buf, "G", total_len, -1, off, len);
		else
			total_len += proc_append_buffer(buf, " ", total_len, -1, off, len);
		if(r->flags & ROUTE_FLAG_DEFAULT)
			total_len += proc_append_buffer(buf, "D", total_len, -1, off, len);
		else
			total_len += proc_append_buffer(buf, " ", total_len, -1, off, len);

		total_len += proc_append_buffer(buf, "  ", total_len, -1, off, len);

		total_len += proc_append_buffer(buf, r->interface->name, total_len, -1, off, len);
		total_len += proc_append_buffer(buf, "\n", total_len, -1, off, len);
	}
	rwlock_release(&table->rwl, RWL_READER);
	return total_len;
}


