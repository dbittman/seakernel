#include <sea/config.h>
#include <sea/net/ethernet.h>
#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/mm/kmalloc.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>
#include <sea/net/ipv4.h>
#include <sea/net/arp.h>
#include <sea/net/route.h>
#include <sea/tm/schedule.h>
#include <sea/net/datalayer.h>

struct llist *net_list;

void net_init()
{
	net_list = ll_create(0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(net_add_device);
	loader_add_kernel_symbol(net_notify_packet_ready);
	loader_add_kernel_symbol(net_receive_packet);
#endif
	arp_init();
	ipv4_init();
}

static int kt_packet_rec_thread(struct kthread *kt, void *arg)
{
	struct net_packet *pack;
	struct net_dev *nd = arg;
	int packets=0;
	while(!kthread_is_joining(kt)) {
		if(nd->rx_pending) {
			packets++;
			TRACE(0, "[kpacket]: got packet (%d %d)\n", nd->rx_pending, packets);
			pack = kmalloc(sizeof(struct net_packet));
			net_callback_poll(nd, pack, 1);
			sub_atomic(&nd->rx_pending, 1);
			net_receive_packet(nd, pack, 1);
		} else {
			//tm_process_pause(current_task);
			tm_schedule();
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
	memcpy(nd->hw_address, mac, sizeof(uint8_t) * 6);
	kthread_create(&nd->rec_thread, "[kpacket]", 0, kt_packet_rec_thread, nd);
	nd->rec_thread.process->priority = 100;
	unsigned char ifa[4];
	ifa[0] = 2;
	ifa[1] = 0;
	ifa[2] = 0;
	ifa[3] = 0xa;
	nd->net_address_len = 4;
	nd->hw_address_len = 6;
	nd->hw_type = 1;
	nd->data_header_len = sizeof(struct ethernet_header);
	net_iface_set_network_addr(nd, 0x800, ifa);
	struct route *r = kmalloc(sizeof(struct route));
	r->interface = nd;
	net_iface_set_flags(nd, IFACE_FLAG_UP);
	r->flags |= ROUTE_FLAG_DEFAULT | ROUTE_FLAG_UP;
	net_route_add_entry(r);
	return nd;
}

void net_remove_device(struct net_dev *nd)
{
	ll_remove(net_list, nd->node);
	kfree(nd);
}


void net_iface_set_network_addr(struct net_dev *nd, int type, uint8_t *addr)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	memcpy(nd->net_address, addr, 4);
}

void net_iface_get_network_addr(struct net_dev *nd, int type, uint8_t *addr)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	memcpy(addr, nd->net_address, 4);
}

int net_iface_get_flags(struct net_dev *nd)
{
	return nd->flags;
}

int net_iface_set_flags(struct net_dev *nd, int flags)
{
	return (nd->flags = net_callback_set_flags(nd, flags));
}

