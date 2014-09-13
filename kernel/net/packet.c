#include <sea/cpu/atomic.h>
#include <sea/net/packet.h>
#include <sea/ll.h>
#include <sea/tm/process.h>
#include <sea/loader/symbol.h>
#include <sea/asm/system.h>
#include <sea/tm/schedule.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/tm/kthread.h>
#include <sea/net/interface.h>
#include <sea/net/datalayer.h>

struct net_packet *net_packet_create(struct net_packet *packet, int flags)
{
	if(!packet) {
		packet = kmalloc(sizeof(struct net_packet));
		packet->flags = (flags | NP_FLAG_ALLOC);
	} else {
		packet->flags = flags;
	}
	TRACE(0, "[packet]: creating new packet %x\n", packet);
	packet->count = 1;
	packet->data_header = packet->data;
	return packet;
}

void net_packet_destroy(struct net_packet *packet)
{
	assert(packet->count == 0);
	TRACE(0, "[packet]: destroying packet %x\n", packet);
	if(packet->flags & NP_FLAG_ALLOC)
		kfree(packet);
}

void net_packet_get(struct net_packet *packet)
{
	TRACE(0, "[packet]: inc ref count packet %x\n", packet);
	assert(packet->count > 0);
	add_atomic(&packet->count, 1);
}

void net_packet_put(struct net_packet *packet, int flag)
{
	TRACE(0, "[packet]: dec ref count packet %x\n", packet);
	assert(packet->count > 0);
	int r = sub_atomic(&packet->count, 1);
	if((flag & NP_FLAG_DESTROY) && r)
		panic(0, "failed to destroy packet before it went out of scope");
	assert(r >= 0);
	if(!r)
		net_packet_destroy(packet);
}

void net_notify_packet_ready(struct net_dev *nd)
{
	add_atomic(&nd->rx_pending, 1);
	tm_process_resume(nd->rec_thread.process);
	/* TODO: notify CPU to schedule this process NOW */
}

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	TRACE(0, "[packet]: receive %d packets\n", count);
	for(int i=0;i<count;i++)
		net_data_receive(nd, &packets[i]);
}

int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	add_atomic(&nd->tx_count, 1);
	TRACE(0, "[packet]: send #%d\n", nd->tx_count);
	int ret = net_callback_send(nd, packets, count);
	TRACE(0, "[packet]: send returned %d\n", ret);
	return ret;
}

