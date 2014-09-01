#include <sea/cpu/atomic.h>
#include <sea/net/packet.h>
#include <sea/ll.h>
#include <sea/tm/process.h>
#include <sea/loader/symbol.h>
#include <sea/asm/system.h>
#include <sea/net/ethernet.h>
#include <sea/tm/schedule.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/tm/kthread.h>
#include <sea/net/interface.h>
void net_notify_packet_ready(struct net_dev *nd)
{
	add_atomic(&nd->rx_pending, 1);
	tm_process_resume(nd->rec_thread.process);
	/* TODO: notify CPU to schedule this process NOW */
}

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	printk(0, "[packet]: receive %d packets\n", count);
	for(int i=0;i<count;i++)
		ethernet_receive_packet(nd, &packets[i]);
}

int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	add_atomic(&nd->tx_count, 1);
	printk(0, "[packet]: send #%d\n", nd->tx_count);
	return net_callback_send(nd, packets, count);
}

int net_block_for_packets(struct net_dev *nd, struct net_packet *packets, int max)
{
	int ret=0;
	do {
		if(!(nd->flags & ND_RX_POLLING)) {
			while(!nd->rx_pending)
				tm_schedule();
		} else
			tm_schedule();
	} while(!(ret=net_callback_poll(nd, packets, max)));
	return ret;
}

