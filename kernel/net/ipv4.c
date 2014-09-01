#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/ipv4.h>
#include <sea/net/icmp.h>
#include <sea/net/route.h>
#include <sea/net/ethernet.h>
#include <sea/net/arp.h>
#include <sea/tm/kthread.h>
#include <sea/tm/process.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>
#include <sea/cpu/time.h>
#include <sea/mm/kmalloc.h>

/* TODO: real queue implementation */
static struct llist *ipv4_send_queue = 0;
static struct kthread *ipv4_send_thread = 0;

static uint16_t ipv4_calc_checksum(void *__data, int length)
{
	uint8_t *data = __data;
	uint32_t sum = 0xFFFF;
	int i;
	for(i=0;i+1<length;i+=2) {
		sum += BIG_TO_HOST16(*(uint16_t *)(data + i));
		if(sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return HOST_TO_BIG16(~sum);
}

void ipv4_accept_packet(struct net_dev *nd, struct ipv4_header *packet,
		union ipv4_address src, int payload_size)
{
	switch(packet->ptype) {
		case IP_PROTOCOL_ICMP:
			icmp_receive_packet(nd, src, (struct icmp_packet *)packet->data, payload_size);
			break;
		default:
			printk(0, "[ipv4]: unknown protocol %x\n", packet->ptype);
	}
}

void ipv4_receive_packet(struct net_dev *nd, struct ipv4_header *packet)
{
	/* check if we are to accept this packet */
	printk(0, "[ipv4]: receive_packet\n");
	uint16_t checksum = packet->checksum;
	packet->checksum = 0;
	uint16_t sum = ipv4_calc_checksum(packet, packet->header_len * 4);
	if(sum != checksum) {
		printk(0, "[ipv4]: discarding bad packet!\n");
		return;
	}
	/* check IP address */
	union ipv4_address src = (union ipv4_address)(uint32_t)(BIG_TO_HOST32(packet->src_ip));
	union ipv4_address dest = (union ipv4_address)(uint32_t)(BIG_TO_HOST32(packet->dest_ip));
	printk(0, "[ipv4]: packet from %x (%d.%d.%d.%d)\n",
			src.address, src.addr_bytes[3], src.addr_bytes[2], src.addr_bytes[1], src.addr_bytes[0]);
	union ipv4_address ifaddr;
	net_iface_get_prot_addr(nd, ETHERTYPE_IPV4, ifaddr.addr_bytes);
	/* TODO: also check mode of interface. accept broadcast packets? etc */
	if(ifaddr.address == dest.address) {
		/* accepted unicast packet for us */
		printk(0, "[ipv4]: got packet for us of size %d!\n", BIG_TO_HOST16(packet->length));
		ipv4_accept_packet(nd, packet, src, BIG_TO_HOST16(packet->length) - (packet->header_len * 4));
	}
	/* TODO: IP forwarding */
}

static int ipv4_do_enqueue_packet(struct ipv4_packet *packet)
{
	/* error checking */
	ll_insert(ipv4_send_queue, packet);
	tm_process_resume(ipv4_send_thread->process);
	return 0;
}

static void ipv4_finish_constructing_packet(struct net_dev *nd, struct route *r, struct ipv4_packet *packet)
{
	union ipv4_address src;
	net_iface_get_prot_addr(nd, ETHERTYPE_IPV4, src.addr_bytes);
	packet->header.src_ip = HOST_TO_BIG32(src.address);
	packet->header.checksum = 0;
	packet->header.checksum = ipv4_calc_checksum(&packet->header, packet->header.header_len * 4);
}

static int ipv4_send_packet(struct ipv4_packet *packet)
{
	struct net_dev *nd;
	union ipv4_address packet_destination;
	union ipv4_address dest = (union ipv4_address)BIG_TO_HOST32(packet->header.dest_ip);
	
	//if(packet->tries > 0 && (time_get_epoch() == packet->last_attempt_time))
	//	return 0;
	packet->tries++;
	packet->last_attempt_time = time_get_epoch();

	struct route *r = net_route_select_entry(dest);
	uint8_t hwaddr[6];
	if(!r)
		return -1;//TODO: NETWORK_UNREACHABLE;
	nd = r->interface;
	if(r->flags & ROUTE_FLAG_GATEWAY) {
		packet_destination = r->gateway;
	} else {
		packet_destination = dest;
	}
	if(arp_lookup(ETHERTYPE_IPV4, packet_destination.addr_bytes, hwaddr) == -ENOENT) {
		printk(0, "[ipv4]: send_packet: ARP lookup failed, sending request\n");
		/* no idea where the destination is! Send an ARP request. ARP handles multiple
		 * requests to the same address. */
		arp_send_request(nd, ETHERTYPE_IPV4, packet_destination.addr_bytes, 4);
		/* re-enqueue the packet */
		ipv4_do_enqueue_packet(packet);
		return 0;
	}
	printk(0, "[ipv4]: send_packet: sending\n");
	ipv4_finish_constructing_packet(nd, r, packet);
	struct ethernet_header eh;
	ethernet_construct_header(&eh, nd->mac, hwaddr, ETHERTYPE_IPV4);
	ethernet_send_packet(nd, &eh, (unsigned char *)&packet->header, BIG_TO_HOST16(packet->header.length));
	kfree(packet);
	return 1;
}

int ipv4_enqueue_packet(uint8_t *data, int length, int ttl, int id, int ptype, union ipv4_address dest)
{
	/* set packet timestamp for entering send queue, and check if the route is
	 * available */
	struct ipv4_packet *packet = kmalloc(sizeof(struct ipv4_packet) + length);
	packet->enqueue_time = time_get_epoch();
	memcpy(packet->header.data, data, length);
	packet->header.ptype = ptype;
	packet->header.id = HOST_TO_BIG16(id);
	packet->header.ttl = ttl;
	packet->header.length = HOST_TO_BIG16(length + 20);
	packet->header.version = 4;
	packet->header.header_len = 5;
	packet->header.dest_ip = HOST_TO_BIG32(dest.address);
	printk(0, "[ipv4]: enqueue packet to %x\n", dest.address);
	ipv4_do_enqueue_packet(packet);
	return 0;
}

static int ipv4_sending_thread(struct kthread *kt, void *arg)
{
	while(!kthread_is_joining(kt)) {
		if(ipv4_send_queue->num > 0) {
			/* try getting an entry */
			struct ipv4_packet *packet = ll_remove_head(ipv4_send_queue);
			printk(0, "[kipv4-send]: popped packet\n");
			if(packet) {
				/* got packet entry! */
				if(time_get_epoch() > packet->enqueue_time + 20) {
					/* timeout! */
					printk(0, "[kipv4-send]: packet timed out\n");
					kfree(packet);
				} else { 
					int r = ipv4_send_packet(packet);
					printk(0, "[kipv4-send]: send returned %d\n", r);
				}
			}
		}
		//tm_process_pause(current_task);
		tm_schedule();
	}
	return 0;
}

void ipv4_init()
{
	ipv4_send_queue = ll_create(0);
	ipv4_send_thread = kthread_create(0, "[kipv4-send]", 0, ipv4_sending_thread, 0);
	ipv4_send_thread->process->priority = 100;
}

