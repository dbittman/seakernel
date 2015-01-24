#include <sea/config.h>
#include <sea/string.h>
#include <sea/kernel.h>
#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/mm/kmalloc.h>
#include <sea/loader/symbol.h>
#include <sea/cpu/atomic.h>
#include <sea/vsprintf.h>
#include <sea/net/arp.h>
#include <sea/net/route.h>
#include <sea/tm/schedule.h>
#include <sea/net/datalayer.h>
#include <sea/fs/devfs.h>
#include <sea/errno.h>
#include <sea/net/tlayer.h>
#include <sea/net/nlayer.h>
#include <sea/net/data_queue.h>
#include <sea/asm/system.h>
uint16_t af_to_ethertype_map[PF_MAX] = {
	[AF_INET] = 0x800,
};

struct llist *net_list;

int nd_num = 0;
static struct net_dev *devices[256];

extern void net_lo_init();

void net_init()
{
	net_list = ll_create(0);
#if CONFIG_MODULES
	loader_add_kernel_symbol(net_add_device);
	loader_add_kernel_symbol(net_notify_packet_ready);
	loader_add_kernel_symbol(net_receive_packet);
	loader_add_kernel_symbol(net_data_queue_enqueue);
	loader_add_kernel_symbol(socket_set_calls);
	loader_add_kernel_symbol(net_tlayer_sendto_network);
	loader_add_kernel_symbol(net_tlayer_bind_socket);
	loader_add_kernel_symbol(net_tlayer_unbind_socket);
	loader_add_kernel_symbol(net_tlayer_register_protocol);
	loader_add_kernel_symbol(net_tlayer_deregister_protocol);
	loader_add_kernel_symbol(sys_bind);
	loader_add_kernel_symbol(net_route_select_entry);
	loader_add_kernel_symbol(net_packet_create);
	loader_add_kernel_symbol(net_nlayer_register_protocol);
	loader_add_kernel_symbol(net_nlayer_unregister_protocol);
	loader_add_kernel_symbol(net_packet_get);
	loader_add_kernel_symbol(net_packet_put);
	loader_add_kernel_symbol(net_iface_get_netaddr);
	loader_add_kernel_symbol(net_iface_get_netmask);
	loader_add_kernel_symbol(net_tlayer_recvfrom_network);
	loader_add_kernel_symbol(arp_lookup);
	loader_add_kernel_symbol(arp_send_request);
	loader_add_kernel_symbol(net_data_send);
	loader_add_kernel_symbol(net_data_register_protocol);
	loader_add_kernel_symbol(net_transmit_packet);
	loader_add_kernel_symbol(net_nlayer_receive_from_dlayer);
	loader_add_kernel_symbol(net_data_unregister_protocol);
#endif
	arp_init();
	net_tlayer_init();
	net_nlayer_init();
	net_data_init();
}

static int kt_packet_rec_thread(struct kthread *kt, void *arg)
{
	struct net_packet *pack;
	struct net_dev *nd = arg;
	int packets=0;
	int ret = 0;
	while(!kthread_is_joining(kt)) {
		if(nd->rx_pending || ret) {
			packets++;
			pack = net_packet_create(0, 0);
			ret = net_callback_poll(nd, pack, 1);
			TRACE(0, "[kpacket]: got packet (%d %d : %d)\n", nd->rx_pending, packets, ret);
			if(ret > 0) {
				if(nd->rx_pending > 0)
					sub_atomic(&nd->rx_pending, 1);
				net_receive_packet(nd, pack, 1);
			} else {
				tm_schedule();
			}
			net_packet_put(pack, 0);
			pack = 0;
			nd->rx_thread_lastwork = tm_get_ticks();
		} else {
			if(tm_get_ticks() > nd->rx_thread_lastwork + TICKS_SECONDS(5))
				tm_process_pause(current_task);
			else
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
	if(fn->poll) {
		kthread_create(&nd->rec_thread, "[kpacket]", 0, kt_packet_rec_thread, nd);
		nd->rec_thread.process->priority = 100;
	}
	net_iface_set_flags(nd, IFACE_FLAGS_DEFAULT);
	int num = add_atomic(&nd_num, 1);
	if(num > 255)
		panic(0, "cannot add new netdev");
	snprintf(nd->name, 16, "nd%d", num);
	nd->num = num;
	devices[num] = nd;
	char path[5 + strlen(nd->name)];
	snprintf(path, 5+strlen(nd->name), "/dev/%s", nd->name);
	sys_mknod(path, S_IFCHR | 600, GETDEV(6, num));

	return nd;
}

void net_remove_device(struct net_dev *nd)
{
	devices[nd->num] = 0;
	ll_remove(net_list, nd->node);
	if(nd->callbacks->poll)
		kthread_join(&nd->rec_thread, 0);
	kfree(nd);
}

void net_iface_set_netmask(struct net_dev *nd, sa_family_t af, struct sockaddr *mask)
{
	memcpy(&nd->masks[af], mask, sizeof(*mask));
}

void net_iface_get_netmask(struct net_dev *nd, sa_family_t af, struct sockaddr *mask)
{
	memcpy(mask, &nd->masks[af], sizeof(*mask));
}

void net_iface_set_bcast_addr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr)
{
	memcpy(&nd->broadcasts[af], addr, sizeof(*addr));
}

void net_iface_get_bcast_addr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr)
{
	memcpy(addr, &nd->broadcasts[af], sizeof(*addr));
}

void net_iface_set_netaddr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr)
{
	memcpy(&nd->addresses[af], addr, sizeof(*addr));
}

void net_iface_get_netaddr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr)
{
	memcpy(addr, &nd->addresses[af], sizeof(*addr));
}

int net_iface_get_flags(struct net_dev *nd)
{
	return nd->flags;
}

int net_iface_set_flags(struct net_dev *nd, int flags)
{
	return (nd->flags = net_callback_set_flags(nd, flags));
}

void net_iface_export_data(struct net_dev *nd, struct if_data *stat)
{
	stat->ifi_type = nd->hw_type;
	stat->ifi_addrlen = 4 /* TODO */;
	stat->ifi_mtu = nd->mtu;
	stat->ifi_baudrate = nd->brate;
	stat->ifi_ipackets = nd->rx_count;
	stat->ifi_ierrors = nd->rx_err_count;
	stat->ifi_opackets = nd->tx_count;
	stat->ifi_oerrors = nd->tx_err_count;
	stat->ifi_ibytes = nd->rx_bytes;
	stat->ifi_obytes = nd->tx_bytes;
	stat->ifi_iqdrops = nd->dropped;
	stat->ifi_collisions = nd->collisions;
}

int net_char_select(int min, int rw)
{
	return 1;
}

int net_char_rw(int rw, int min, char *buf, size_t count)
{
	return -ENOTSUP;
}

int __get_addr_len(sa_family_t af)
{
	switch(af) {
		case AF_INET:
			return 4;
	}
	return 0;
}

struct ul_route {
	struct sockaddr dest, gate, mask;
	int flags;
};

int net_char_ioctl(dev_t min, int cmd, long arg)
{
	struct net_dev *nd = devices[min];
	if(!nd)
		return -EINVAL;
	struct ifreq *req = (void *)arg;
	struct if_data *stat = (void *)arg;
	struct ul_route *rt = (void *)arg;
	struct sockaddr *sa = (struct sockaddr *)(&req->ifr_addr);
	uint32_t mask;
	int flags;
	struct route *route;
	switch(cmd) {
		case SIOCGIFCOUNT:
			/* TODO: uhg */
			req->ifr_index = net_list->num;
			break;
		case SIOCGIFNAME:
			flags = req->ifr_index;
			int i;
			for(i=0;i<256;i++) {
				if(devices[i] && !(flags--)) {
					break;
				}
			}
			if(i == 256)
				return -EINVAL;
			strncpy(req->ifr_name, devices[i]->name, IFNAMSIZ);
			break;
		case SIOCSIFADDR:
			net_iface_set_netaddr(nd, sa->sa_family, sa);
			nd->netaddr_lengths[sa->sa_family] = __get_addr_len(sa->sa_family);
			break;
		case SIOCGIFADDR:
			net_iface_get_netaddr(nd, sa->sa_family, sa);
			break;
		case SIOCSIFNETMASK:
			memcpy(&mask, sa->sa_data + 2, 4);
			net_iface_set_netmask(nd, sa->sa_family, sa);
			break;
		case SIOCGIFNETMASK:
			net_iface_get_netmask(nd, sa->sa_family, sa);
			break;
		case SIOCGIFFLAGS:
			req->ifr_flags = net_iface_get_flags(nd);
			break;
		case SIOCSIFFLAGS:
			flags = req->ifr_flags;
			flags &= ~IFACE_FLAGS_READONLY;
			int old_flags = net_iface_get_flags(nd);
			flags |= (IFACE_FLAGS_READONLY & old_flags);
			net_iface_set_flags(nd, flags);
			break;
		case SIOCADDRT:
			route = kmalloc(sizeof(struct route));
			memcpy(&mask, rt->gate.sa_data + 2, 4);
			route->gateway = mask;
			memcpy(&mask, rt->dest.sa_data + 2, 4);
			route->destination = mask;
			memcpy(&mask, rt->mask.sa_data + 2, 4);
			route->netmask = mask;
			route->interface = nd;
			route->flags = rt->flags;
			net_route_add_entry(route);
			break;
		case SIOCDELRT:
			memcpy(&mask, rt->dest.sa_data + 2, 4);
			net_route_find_del_entry(mask, nd);
			break;
		case SIOCGIFHWADDR:
			memcpy(sa->sa_data, nd->hw_address, nd->hw_address_len);
			break;
		case SIOCGIFMTU:
			req->ifr_mtu = nd->mtu;
			break;
		case SIOCSIFMTU:
			if(current_task->thread->effective_uid != 0)
				return -EPERM;
			nd->mtu = req->ifr_mtu;
			break;
		case SIOCGIFDATA: 
			net_iface_export_data(nd, stat);
			break;
		case SIOCSIFBRDADDR:
			net_iface_set_bcast_addr(nd, sa->sa_family, sa);
			break;
		case SIOCGIFBRDADDR:
			net_iface_get_bcast_addr(nd, sa->sa_family, sa);
			break;
		default:
			return -EOPNOTSUPP;
	}
	return 0;
}

