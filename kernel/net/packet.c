#include <sea/asm/system.h>
#include <sea/kernel.h>
#include <sea/lib/linkedlist.h>
#include <sea/loader/symbol.h>
#include <sea/mm/kmalloc.h>
#include <sea/net/datalayer.h>
#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/string.h>
#include <sea/tm/kthread.h>
#include <sea/tm/process.h>
#include <sea/vsprintf.h>
#include <stdatomic.h>
#include <sea/trace.h>
#include <sea/kobj.h>

struct net_packet *net_packet_create(struct net_packet *packet, int flags)
{
	KOBJ_CREATE(packet, flags, NP_FLAG_ALLOC);
	TRACE_MSG("net.packet", "creating new packet %x\n", packet);
	packet->count = 1;
	packet->data_header = packet->data;
	return packet;
}

void net_packet_destroy(struct net_packet *packet)
{
	assert(packet->count == 0);
	TRACE_MSG("net.packet", "destroying packet %x\n", packet);
	KOBJ_DESTROY(packet, NP_FLAG_ALLOC);
}

void net_packet_get(struct net_packet *packet)
{
	TRACE_MSG("net.packet", "inc ref count packet %x\n", packet);
	assert(packet->count > 0);
	atomic_fetch_add(&packet->count, 1);
}

void net_packet_put(struct net_packet *packet, int flag)
{
	TRACE_MSG("net.packet", "dec ref count packet %x\n", packet);
	assert(packet->count > 0);
	int r = atomic_fetch_sub(&packet->count, 1) - 1;
	if((flag & NP_FLAG_DESTROY) && r)
		panic(0, "failed to destroy packet before it went out of scope");
	assert(r >= 0);
	if(!r)
		net_packet_destroy(packet);
}

void net_notify_packet_ready(struct net_dev *nd)
{
	atomic_fetch_add(&nd->rx_pending, 1);
	if(nd->callbacks->poll)
		tm_thread_resume(nd->rec_thread.thread);
	/* TODO: notify CPU to schedule this process NOW */
}

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	TRACE_MSG("net.packet", "receive %d packets\n", count);
	atomic_fetch_add_explicit(&nd->rx_count, count, memory_order_relaxed);
	for(int i=0;i<count;i++) {
		atomic_fetch_add_explicit(&nd->rx_bytes, packets[i].length, memory_order_relaxed);
		net_data_receive(nd, &packets[i]);
	}
}

int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	atomic_fetch_add_explicit(&nd->tx_count, count, memory_order_relaxed);
	for(int i=0;i<count;i++)
		atomic_fetch_add_explicit(&nd->tx_bytes, packets[i].length, memory_order_relaxed);
	TRACE_MSG("net.packet", "send #%d\n", nd->tx_count);
	int ret = net_callback_send(nd, packets, count);
	TRACE_MSG("net.packet", "send returned %d\n", ret);
	return ret;
}

