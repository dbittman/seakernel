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
#include <sea/tm/schedule.h>

#include <sea/cpu/time.h>
#include <sea/cpu/atomic.h>

#include <sea/mm/kmalloc.h>
#include <sea/lib/queue.h>

#include <sea/ll.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>

#include <limits.h>

#include <modules/ipv4/ipv4sock.h>
#include <modules/ipv4/ipv4.h>
#include <modules/ipv4/icmp.h>

struct queue *ipv4_tx_queue = 0;
struct kthread *ipv4_send_thread = 0;
time_t ipv4_thread_lastwork;

static int ipv4_do_enqueue_packet(struct ipv4_packet *packet)
{
	/* error checking */
	queue_enqueue(ipv4_tx_queue, packet);
	tm_process_resume(ipv4_send_thread->process);
	return 0;
}

static void ipv4_finish_constructing_packet(struct net_dev *nd, struct route *r, struct ipv4_packet *packet)
{
	if(!(packet->netpacket->flags & NP_FLAG_FORW) && !(packet->netpacket->flags & NP_FLAG_NOFILLSRC)) {
		union ipv4_address src;
		net_iface_get_network_addr(nd, ETHERTYPE_IPV4, src.addr_bytes);
		packet->header->src_ip = src.address;
	}
	packet->header->version = 4;
	packet->header->header_len = 5;
	packet->header->frag_offset = 0;
	packet->header->checksum = 0;
	packet->header->checksum = ipv4_calc_checksum(packet->header, packet->header->header_len * 4);
}

static int ipv4_resolve_hwaddr(struct net_dev *nd, int ethertype, uint8_t *netaddr, uint8_t *hwaddr, int flags)
{
	if(flags & NLAYER_FLAG_HW_BROADCAST) {
		memset(hwaddr, 0xFF, 6);
	} else {
		if(arp_lookup(ETHERTYPE_IPV4, netaddr, hwaddr) == -ENOENT) {
			/* no idea where the destination is! Send an ARP request. ARP handles multiple
			 * requests to the same address. */
			arp_send_request(nd, ETHERTYPE_IPV4, netaddr, 4);
			return -ENOENT;
		}
	}
	return 0;
}

static int ipv4_send_packet(struct ipv4_packet *packet)
{
	struct net_dev *nd;
	union ipv4_address packet_destination;
	union ipv4_address dest = (union ipv4_address)packet->header->dest_ip;
	
	if(packet->tries > 0 && (tm_get_ticks() <= packet->last_attempt_time)) {
		ipv4_do_enqueue_packet(packet);
		return 0;
	}
	packet->tries++;
	packet->last_attempt_time = tm_get_ticks();

	struct route *r = net_route_select_entry(dest.address);
	uint8_t hwaddr[6];
	if(!r)
		return -1;
	nd = r->interface;
	if(r->flags & ROUTE_FLAG_GATEWAY) {
		packet_destination = (union ipv4_address)r->gateway;
	} else {
		packet_destination = dest;
	}
	union ipv4_address ifaddr;
	net_iface_get_network_addr(nd, ETHERTYPE_IPV4, ifaddr.addr_bytes);
	if(packet_destination.address == ifaddr.address) {
		/* we're sending a packet to an address of an interface on this system! Loop it around... */
		TRACE(0, "[ipv4]: sending packet to interface on this system! Re-receiving...\n");
		ipv4_finish_constructing_packet(nd, r, packet);
		ipv4_receive_packet(nd, packet->netpacket, packet->header);
		return 1;
	}
	/* try to resolve the hardware address */
	int nl_flags = 0;
	if(dest.address == BROADCAST_ADDRESS(ifaddr.address, nd->netmask))
		nl_flags = NLAYER_FLAG_HW_BROADCAST;
	if(ipv4_resolve_hwaddr(nd, ETHERTYPE_IPV4, packet_destination.addr_bytes, hwaddr, nl_flags) == -ENOENT) {
		if(packet->tries > 5)
			packet->last_attempt_time = tm_get_ticks() + 20;
		ipv4_do_enqueue_packet(packet);
		return 0;
	}

	TRACE(0, "[ipv4]: send_packet: sending\n");
	ipv4_finish_constructing_packet(nd, r, packet);
	
	/* split the packet if we need to... */
	int total_packet_length = r->interface->data_header_len + BIG_TO_HOST16(packet->header->length);
	int data_length = BIG_TO_HOST16(packet->header->length) - packet->header->header_len * 4;
	uint16_t parent_flags = (BIG_TO_HOST16(packet->header->frag_offset) & 0xF000) >> 12;
	uint16_t parent_offset = (BIG_TO_HOST16(packet->header->frag_offset) & ~0xF000) * 8;
	if(r->interface->mtu && r->interface->mtu < total_packet_length) {
		if(parent_flags & IP_FLAG_DF) {
			add_atomic(&nd->dropped, 1);
			/* TODO: ICMP back */
			return -1;
		}
		/* split! */
		int multiples = (nd->mtu - packet->header->header_len * 4) / 8;
		int num_frags = data_length / multiples + 1;
		assert(num_frags >= 2);
		for(int i = 1;i<num_frags;i++) {
			struct net_packet *frag = net_packet_create(0, 0);
			struct ipv4_header *fh = (void *)(frag->data + nd->data_header_len);
			memcpy(fh, packet->header, sizeof(*fh));

			/* copy the data... */
			int offset = i * multiples;
			int length = multiples;
			if(offset + length > data_length)
				length = data_length - offset;
			memcpy(fh->data, packet->header->data + offset, length);
			/* fix up the new header */

			uint16_t flags = ((i + 1 == num_frags) && !(parent_flags & IP_FLAG_MF))
				? 0 : IP_FLAG_MF;
			fh->frag_offset = HOST_TO_BIG16(
					((uint16_t)((offset + parent_offset) / 8) & ~0xF000) | (flags << 12)
					);

			/* enqueue packet */
			ipv4_enqueue_packet(frag, fh);
			net_packet_put(frag, 0);
		}
		/* parent packet header update */
		packet->header->frag_offset = HOST_TO_BIG16(IP_FLAG_MF << 12);
	}

	net_data_send(nd, packet->netpacket, AF_INET, hwaddr, BIG_TO_HOST16(packet->header->length));
	return 1;
}

int ipv4_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header)
{
	union ipv4_address dest = (union ipv4_address)header->dest_ip;
	struct route *r = net_route_select_entry(dest.address);
	if(!r) {
		TRACE(0, "[ipv4]: destination unavailable\n");
		return -ENETUNREACH;
	}
	struct ipv4_packet *packet = kmalloc(sizeof(struct ipv4_packet));
	packet->enqueue_time = tm_get_ticks();
	packet->header = header;
	net_packet_get(netpacket);
	packet->netpacket = netpacket;
	TRACE(0, "[ipv4]: enqueue packet to %x\n", header->dest_ip);
	ipv4_do_enqueue_packet(packet);
	return 0;
}

int ipv4_copy_enqueue_packet(struct net_packet *netpacket, struct ipv4_header *header)
{
	union ipv4_address dest = (union ipv4_address)header->dest_ip;
	struct route *r = net_route_select_entry(dest.address);
	if(!r) {
		TRACE(0, "[ipv4]: destination unavailable\n");
		return -ENETUNREACH;
	}
	memcpy(netpacket->data + r->interface->data_header_len, header, BIG_TO_HOST16(header->length));
	struct ipv4_packet *packet = kmalloc(sizeof(struct ipv4_packet));
	packet->enqueue_time = tm_get_ticks();
	packet->header = (void *)(netpacket->data + r->interface->data_header_len);
	net_packet_get(netpacket);
	packet->netpacket = netpacket;
	TRACE(0, "[ipv4]: enqueue packet to %x\n", header->dest_ip);
	ipv4_do_enqueue_packet(packet);
	return BIG_TO_HOST32(header->length);
}

int ipv4_enqueue_sockaddr(void *payload, size_t len, struct sockaddr *addr, struct sockaddr *src, int prot)
{
	union ipv4_address dest, src_ip;
	memcpy(&dest.address, addr->sa_data + 2, 4);
	struct route *r = net_route_select_entry(dest.address);
	if(!r)
		return -ENETUNREACH;

	struct net_packet *np = net_packet_create(0, 0);
	struct ipv4_header *header = (void *)(np->data + r->interface->data_header_len);

	header->dest_ip = addr->sa_data[2] | (addr->sa_data[3] << 8)
		| (addr->sa_data[4] << 16) | (addr->sa_data[5] << 24);
	header->ttl = 64;
	header->length = HOST_TO_BIG16(len + 20);
	header->id = 0;
	header->ptype = prot;
	memcpy(&src_ip.address, src->sa_data + 2, 4);
	if(src_ip.address) {
		/* we're given a specific src address, so copy it in */
		header->src_ip = src_ip.address;
		np->flags |= NP_FLAG_NOFILLSRC;
	}

	memcpy(header->data, payload, len);
	struct ipv4_packet *packet = kmalloc(sizeof(struct ipv4_packet));
	packet->enqueue_time = tm_get_ticks();
	packet->header = header;
	packet->netpacket = np;
	TRACE(0, "[ipv4]: enqueue packet to %x\n", header->dest_ip);
	ipv4_do_enqueue_packet(packet);
	return 0;
}

int ipv4_sending_thread(struct kthread *kt, void *arg)
{
	while(!kthread_is_joining(kt)) {
		if(queue_count(ipv4_tx_queue) > 0) {
			/* try getting an entry */
			struct ipv4_packet *packet = queue_dequeue(ipv4_tx_queue);
			if(packet) {
				/* got packet entry! */
				if(tm_get_ticks() > packet->enqueue_time + TICKS_SECONDS(10)) {
					/* timeout! */
					TRACE(0, "[kipv4-send]: packet timed out\n");
					net_packet_put(packet->netpacket, 0);
					kfree(packet);
				} else { 
					int r = ipv4_send_packet(packet);
					if(!r)
						tm_schedule();
					if(r != 0) {
						net_packet_put(packet->netpacket, 0);
						kfree(packet);
					}
				}
			}
			ipv4_thread_lastwork = tm_get_ticks();
		}
		if(tm_get_ticks() > ipv4_thread_lastwork + TICKS_SECONDS(5))
			tm_process_pause(current_task);
		else if(!queue_count(ipv4_tx_queue))
			tm_schedule();
		__ipv4_cleanup_fragments(0);
	}
	return 0;
}


