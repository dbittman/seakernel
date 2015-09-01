#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/nlayer.h>
#include <sea/net/route.h>
#include <sea/net/datalayer.h>
#include <sea/net/ethertype.h>
#include <sea/net/arp.h>
#include <sea/net/tlayer.h>

#include <sea/tm/kthread.h>
#include <sea/tm/process.h>
#include <sea/tm/timing.h>

#include <sea/cpu/time.h>
#include <sea/cpu/atomic.h>

#include <sea/mm/kmalloc.h>
#include <sea/lib/queue.h>

#include <sea/ll.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/kernel.h>

#include <limits.h>

#include <modules/ipv4/ipv4sock.h>
#include <modules/ipv4/ipv4.h>
#include <modules/ipv4/icmp.h>

struct llist *frag_list;

static struct ipv4_fragment *__ipv4_find_fragment(struct ipv4_header *header)
{
	rwlock_acquire(&frag_list->rwl, RWL_READER);
	struct llistnode *node;
	struct ipv4_fragment *f;
	ll_for_each_entry(frag_list, node, struct ipv4_fragment *, f) {
		if(f->src == header->src_ip
				&& f->dest == header->dest_ip
				&& f->id == header->id
				&& f->prot == header->ptype
				&& tm_timing_get_microseconds() < f->start_time + ONE_SECOND * FRAG_TIMEOUT) {
			rwlock_release(&frag_list->rwl, RWL_READER);
			return f;
		}
	}
	rwlock_release(&frag_list->rwl, RWL_READER);
	return 0;
}

/* below is the simple algorithm for IP fragmentation reconstruction, based on RFC815.
 * It runs reasonably fast, (each fragment costs O(n) when fixing up the resources),
 * so it should be okay for most situation (unless you've got a router doing something
 * incredibly stupid). */

struct hole {
	uint16_t first;
	uint16_t last;
	int16_t next;
	uint16_t deleted;
} __attribute__((packed));

static void __ipv4_add_fragment(struct ipv4_fragment *frag, struct ipv4_header *header, int offset, int flags)
{
	assert(!frag->complete);
	/* the hole list is stored inside the packet data. Since a fragment must be in multiples
	 * of 8 bytes, a hole will always be at least 8 bytes long (see RFC815). */
	uint8_t *data = (uint8_t *)frag->netpacket->network_header + header->header_len * 4;
	size_t frag_len = header->length - header->header_len * 4;
	/* copy in the header */
	if(!offset) {
		memcpy(frag->netpacket->network_header, header, header->header_len * 4);
	}
	struct hole *hole = (void *)(frag->first_hole + data), *prev = 0;
	int frag_first = offset;
	int frag_last = offset + frag_len - 1;
	while(hole) {
		/* if we this hole is not affected, skip it */
		if(frag_first > hole->last || frag_last < hole->first) {
			prev = hole;
			if(hole->next == -1)
				hole = 0;
			else
				hole = (void *)(data + hole->next);
			continue;
		}
		/* delete the hole */
		if(prev)
			prev->next = hole->next;
		else
			frag->first_hole = hole->next;
		hole->deleted = 1;
		int16_t next = hole->next;
		/* if we're offset from this hole start, we un-delete it and change
		 * it's size. If the start of the fragment is equal to the start
		 * of the hole, we would just delete it, which we've already done. */
		if(frag_first > hole->first) {
			if(prev)
				prev->next = hole->first;
			else
				frag->first_hole = hole->first;
			hole->last = frag_first - 1;
			hole->deleted = 0;
		}
		/* if we don't fill up the hole, we'll need to add a new hole descriptor. */
		if(frag_last < hole->last && (flags & IP_FLAG_MF)) {
			struct hole *newhole = (void *)(data + frag_last + 1);
			newhole->first = frag_last + 1;
			newhole->last = hole->last;
			newhole->next = hole->next;
			/* if the hole was deleted, then prev should point to the new one. Otherwise,
			 * hole should point to the new one */
			if(hole->deleted)
				prev->next = newhole->first;
			else
				hole->next = newhole->first;
			/* the next hole to consider is the new one */
			next = newhole->first;
		}
		/* we may need to update the first_hole pointer */
		if(hole->first == frag->first_hole && hole->deleted)
			frag->first_hole = next;
		/* if we've deleted the hole, then prev doesn't change */
		if(!hole->deleted)
			prev = hole;
		/* check if we're at the end */
		if(next == -1)
			hole = 0;
		else
			hole = (void *)(data + next);
	}
	/* hole list is fixed up, copy the data */
	memcpy((void *)(data + offset), header->data, frag_len);
	/* the last packet gives us the total length */
	if(!(flags & IP_FLAG_MF))
		frag->total_length = offset + frag_len;
	/* if the hole list is empty, then we mark the packet as complete */
	if(frag->first_hole == -1)
		frag->complete = 1;
}

static void __ipv4_new_fragment(struct net_packet *np, struct ipv4_header *header)
{
	struct ipv4_fragment *frag = kmalloc(sizeof(struct ipv4_fragment));
	frag->src = header->src_ip;
	frag->dest = header->dest_ip;
	frag->prot = header->ptype;
	frag->id = header->id;
	frag->start_time = tm_timing_get_microseconds();
	net_packet_get(np);
	frag->netpacket = np;
	frag->first_hole = 0;
	uint8_t *data = (uint8_t *)frag->netpacket->network_header + header->header_len * 4;
	struct hole *hole = (void *)data;
	uint16_t flags = (BIG_TO_HOST16(header->frag_offset) & 0xF000) >> 12;
	uint16_t offset = (BIG_TO_HOST16(header->frag_offset) & ~0xF000) * 8;
	hole->first = 0;
	hole->last = 0xFFFF;
	hole->next = -1;
	__ipv4_add_fragment(frag, header, offset, flags);
	frag->node = ll_insert(frag_list, frag);
}

int __ipv4_cleanup_fragments(int do_remove)
{
	rwlock_acquire(&frag_list->rwl, RWL_READER);
	struct llistnode *node;
	struct ipv4_fragment *f, *rem = 0;
	ll_for_each_entry(frag_list, node, struct ipv4_fragment *, f) {
		if(do_remove || (!f->complete && (tm_timing_get_microseconds() > f->start_time + ONE_SECOND * (FRAG_TIMEOUT)))) {
			printk(1, "[ipv4]: removing old incomplete fragment\n");
			rem = f;
			break;
		}
	}
	rwlock_release(&frag_list->rwl, RWL_READER);
	if(rem) {
		ll_remove(frag_list, rem->node);
		net_packet_put(rem->netpacket, 0);
		kfree(rem);
		return 1;
	}
	return 0;
}

static int ipv4_handle_fragmentation(struct net_packet **np, struct ipv4_header **head, int *size, int *ff)
{
	/* if this isn't a fragment, return */
	struct ipv4_header *header = *head;
	uint16_t flags = (BIG_TO_HOST16(header->frag_offset) & 0xF000) >> 12;
	uint16_t offset = (BIG_TO_HOST16(header->frag_offset) & ~0xF000) * 8;
	*ff = 0;
	if(offset == 0 && !(flags & IP_FLAG_MF))
		return 1;
	TRACE(0, "[ipv4]: UNTESTED: handling fragmentation\n");
	/* wake up the worker thread (since it handles timed-out fragments */
	tm_thread_resume(ipv4_send_thread->thread);
	/* okay, this is a fragment. Do we have other fragments of this packet yet? */
	struct ipv4_fragment *parent = __ipv4_find_fragment(header);
	if(!parent) {
		/* this is a new packet, waiting for more fragments. Set it up... */
		__ipv4_new_fragment(*np, header);
	} else {
		/* add fragment.. */
		__ipv4_add_fragment(parent, header, offset, flags);
	}
	if(parent->complete) {
		ll_remove(frag_list, parent->node);
		*np = parent->netpacket;
		*head = parent->header;
		*size = parent->total_length - parent->header->header_len * 4;
		*ff = 1;
		return 1;
	}
	return 0;
}

static void ipv4_accept_packet(struct net_dev *nd, struct net_packet *netpacket, struct ipv4_header *packet,
		union ipv4_address src, int payload_size)
{
	int from_fragment = 0;
	if(!ipv4_handle_fragmentation(&netpacket, &packet, &payload_size, &from_fragment))
		return;
	ipv4_copy_to_sockets(netpacket, packet);
	struct sockaddr sa_src, sa_dest;
	switch(packet->ptype) {
		case IP_PROTOCOL_ICMP:
			icmp_receive_packet(nd, netpacket, src, (struct icmp_packet *)packet->data, payload_size);
			break;
		default:
			memset(&sa_src, 0, sizeof(sa_src));
			memset(&sa_dest, 0, sizeof(sa_dest));
			struct sockaddr s;
			net_iface_get_netaddr(nd, AF_INET, &s);
			memcpy(sa_dest.sa_data + 2, s.sa_data + 2, 4);
			memcpy(sa_src.sa_data + 2, &src.address, 4);
			sa_src.sa_family = sa_dest.sa_family = AF_INET;
			net_tlayer_recvfrom_network(&sa_src, &sa_dest, netpacket, packet->ptype, packet->data, payload_size);
			break;
	}
	if(from_fragment)
		net_packet_put(netpacket, 0);
}

void ipv4_forward_packet(struct net_dev *nd, struct net_packet *netpacket, struct ipv4_header *header)
{
	if(!(header->ttl--)) {
		add_atomic(&nd->dropped, 1);
		/* send an ICMP message back */
		struct net_packet *resp = net_packet_create(0, 0);
		unsigned char data[header->header_len * 8 + 8 + sizeof(struct icmp_packet)];
		struct ipv4_header *rh = (void *)data;
		rh->ttl = 64;
		rh->dest_ip = header->src_ip;
		rh->id = 0;
		rh->length = HOST_TO_BIG16(header->header_len * 8 + 8 + sizeof(struct icmp_packet));
		rh->ptype = 1;
		memcpy(data + header->header_len * 4 + sizeof(struct icmp_packet), header, header->header_len * 4 + 8);
		struct icmp_packet *ir = (void *)(data + header->header_len * 4);
		ir->type = 11;
		ir->code = 0;
		ipv4_copy_enqueue_packet(resp, rh);
		net_packet_put(resp, 0);
		return;
	}
	netpacket->flags |= NP_FLAG_FORW; 
	ipv4_enqueue_packet(netpacket, header);
}

void ipv4_receive_packet(struct net_dev *nd, struct net_packet *netpacket, void *__packet)
{
	/* check if we are to accept this packet */
	TRACE(0, "[ipv4]: receive_packet\n");
	struct ipv4_header *packet = __packet;
	netpacket->network_header = packet;
	uint16_t checksum = packet->checksum;
	packet->checksum = 0;
	uint16_t sum = ipv4_calc_checksum(packet, packet->header_len * 4);
	if(sum != checksum) {
		TRACE(0, "[ipv4]: discarding bad packet!\n");
		return;
	}
	/* check IP address */
	union ipv4_address src = (union ipv4_address)(uint32_t)packet->src_ip;
	union ipv4_address dest = (union ipv4_address)(uint32_t)packet->dest_ip;
	TRACE(0, "[ipv4]: packet from %x (%d.%d.%d.%d)\n",
			src.address, src.addr_bytes[0], src.addr_bytes[1], src.addr_bytes[2], src.addr_bytes[3]);
	union ipv4_address ifaddr;
	uint32_t mask;
	struct sockaddr s;
	net_iface_get_netaddr(nd, AF_INET, &s);
	memcpy(&ifaddr.address, s.sa_data + 2, 4);
	net_iface_get_netmask(nd, AF_INET, &s);
	memcpy(&mask, s.sa_data + 2, 4);
	if(ifaddr.address == dest.address
			/* TODO: should we use this automatic broadcast address? */
			|| (dest.address == BROADCAST_ADDRESS(ifaddr.address, mask)
				&& (nd->flags & IFACE_FLAG_ACCBROADCAST))) {
		/* accepted unicast or broadcast packet for us */
		TRACE(0, "[ipv4]: got packet for us of size %d!\n", BIG_TO_HOST16(packet->length));
		ipv4_accept_packet(nd, netpacket, packet, 
				src, BIG_TO_HOST16(packet->length) - (packet->header_len * 4));
	} else if(nd->flags & IFACE_FLAG_FORWARD || 1/*TODO */) {
		ipv4_forward_packet(nd, netpacket, packet);
	} else {
		add_atomic(&nd->dropped, 1);
	}
}

