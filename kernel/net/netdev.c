#include <sea/config.h>
#include <sea/net/ethernet.h>
#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/mm/kmalloc.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>
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
	struct net_packet pack;
	struct net_dev *nd = arg;
	int packets=0;
	while(!kthread_is_joining(kt)) {
		if(nd->rx_pending) {
			packets++;
			printk(0, "kt rec packet %d: got packet (%d %d)\n", current_task->pid, nd->rx_pending, packets);
			net_callback_poll(nd, &pack, 1);
			sub_atomic(&nd->rx_pending, 1);
			net_receive_packet(nd, &pack, 1);
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
	unsigned char ifa[4];
	ifa[0] = 0xa;
	ifa[1] = 0;
	ifa[2] = 0;
	ifa[3] = 2;
	net_iface_set_prot_addr(nd, 0x800, ifa);
	return nd;
}

void net_remove_device(struct net_dev *nd)
{
	ll_remove(net_list, nd->node);
	kfree(nd);
}


void net_iface_set_prot_addr(struct net_dev *nd, int type, uint8_t *addr)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	memcpy(nd->ipv4, addr, 4);
}

void net_iface_get_prot_addr(struct net_dev *nd, int type, uint8_t *addr)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	memcpy(addr, nd->ipv4, 4);
}

