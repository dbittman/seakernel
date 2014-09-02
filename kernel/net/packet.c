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
	return net_callback_send(nd, packets, count);
}

