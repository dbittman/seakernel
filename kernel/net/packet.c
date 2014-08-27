#include <sea/cpu/atomic.h>
#include <sea/net/net.h>
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
struct llist *net_list;

void net_init()
{
	net_list = ll_create(0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(net_add_device);
	loader_add_kernel_symbol(net_notify_packet_ready);
	loader_add_kernel_symbol(net_block_for_packets);
	loader_add_kernel_symbol(net_receive_packet);
#endif
}

static int kt_packet_rec_thread(struct kthread *kt, void *arg)
{
	struct net_dev *nd = arg;
	while(!kthread_is_joining(kt)) {
		if(nd->rx_pending) {
			printk(0, "kt rec packet %d: got packet\n", current_task->pid);
			sub_atomic(&nd->rx_pending, 1);
		} else {
			tm_process_pause(current_task);
		}
	}
	return 0;
}

struct net_dev *net_add_device(struct net_dev_calls *fn, void *data)
{
	struct net_dev *nd = kmalloc(sizeof(struct net_dev));
	nd->node = ll_insert(net_list, nd);
	nd->callbacks = fn;
	nd->data = data;
	uint8_t mac[6];
	net_callback_get_mac(nd, mac);
	memcpy(nd->mac, mac, sizeof(uint8_t) * 6);
	kthread_create(&nd->rec_thread, "[kpacket]", 0, kt_packet_rec_thread, nd);
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
	tm_process_resume(nd->rec_thread.process);
	/* TODO: notify CPU to schedule this process NOW */
}

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	kprintf("NET: PACKETS: %d\n", count);
	for(int i=0;i<count;i++)
		ethernet_receive_packet(nd, &packets[i]);
}

int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
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


