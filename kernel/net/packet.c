#include <kernel.h>
#include <atomic.h>
#include <net.h>
#include <ll.h>
#include <task.h>
#include <symbol.h>

struct llist *net_list;

void net_init()
{
	net_list = ll_create(0);
#if CONFIG_MODULES
	add_kernel_symbol(net_add_device);
	add_kernel_symbol(net_notify_packet_ready);
	add_kernel_symbol(net_block_for_packets);
	add_kernel_symbol(net_receive_packet);
#endif
}

struct net_dev *net_add_device(struct net_dev_calls *fn, void *data)
{
	struct net_dev *nd = kmalloc(sizeof(struct net_dev));
	nd->node = ll_insert(net_list, nd);
	nd->callbacks = fn;
	nd->data = data;
	return nd;
}

void net_remove_device(struct net_dev *nd)
{
	ll_remove(net_list, nd->node);
	kfree(nd);
}

void net_notify_packet_ready(struct net_dev *nd)
{
	add_atomic(&nd->rx_pending, 1);
}

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	/* all the packets sent to this function must be copied */
}

int net_block_for_packets(struct net_dev *nd, struct net_packet *packets, int max)
{
	int ret=0;
	do {
		if(!(nd->flags & ND_RX_POLLING)) {
			while(!nd->rx_pending)
				schedule();
		} else
			schedule();
	} while(!(ret=net_callback_poll(nd, packets, max)));
	return ret;
}
