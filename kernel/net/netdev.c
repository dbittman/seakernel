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
#include <sea/fs/devfs.h>
#include <sea/errno.h>
struct llist *net_list;

int nd_num = 0;
static struct net_dev *devices[256];

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
	int ret = 0;
	while(!kthread_is_joining(kt)) {
		if(nd->rx_pending || ret) {
			packets++;
			pack = net_packet_create(0, 0);
			ret = net_callback_poll(nd, pack, 1);
			TRACE(0, "[kpacket]: got packet (%d %d : %d)\n", nd->rx_pending, packets, ret);
			if(ret) {
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
	kthread_create(&nd->rec_thread, "[kpacket]", 0, kt_packet_rec_thread, nd);
	nd->rec_thread.process->priority = 100;
	
	
	
	
	net_iface_set_flags(nd, IFACE_FLAG_UP);
	
	
	
	int num = add_atomic(&nd_num, 1);
	if(num > 255)
		panic(0, "cannot add new netdev");
	snprintf(nd->name, 16, "nd%d", num);
	nd->num = num;
	devices[num] = nd;
	nd->devnode = devfs_add(devfs_root, nd->name, S_IFCHR, 6, num);

	return nd;
}

void net_remove_device(struct net_dev *nd)
{
	devices[nd->num] = 0;
	ll_remove(net_list, nd->node);
	kfree(nd);
}

void net_iface_set_network_mask(struct net_dev *nd, int type, uint32_t mask)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	nd->netmask = mask;
}

void net_iface_get_network_mask(struct net_dev *nd, int type, uint32_t *mask)
{
	if(type != 0x800)
		panic(0, "unknown protocol type");
	*mask = nd->netmask;
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

int net_char_select(int a, int b)
{

}

int net_char_rw(int rw, int min, char *buf, size_t count)
{

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
	printk(0, "[netdev]: ioctl %d %d %x\n", min, cmd, arg);
	struct ifreq *req = (void *)arg;
	struct ul_route *rt = (void *)arg;
	struct sockaddr *sa = (struct sockaddr *)(&req->ifr_addr);
	unsigned char ifa[4];
	uint32_t mask;
	struct route *route;
	switch(cmd) {
		case SIOCSIFADDR:
			ifa[0] = sa->sa_data[5];
			ifa[1] = sa->sa_data[4];
			ifa[2] = sa->sa_data[3];
			ifa[3] = sa->sa_data[2];
			printk(0, "setting addr: %x %x %x %x\n", sa->sa_data[2], sa->sa_data[3], sa->sa_data[4], sa->sa_data[5]);
			net_iface_set_network_addr(nd, 0x800, (uint8_t *)(sa->sa_data + 2));
			nd->net_address_len = 4;
			break;
		case SIOCSIFNETMASK:
			memcpy(&mask, sa->sa_data + 2, 4);
			printk(0, "set mask: %x %x %x %x : %x\n", (uint8_t)sa->sa_data[2], (uint8_t)sa->sa_data[3], (uint8_t)sa->sa_data[4], (uint8_t)sa->sa_data[5], BIG_TO_HOST32(mask));
			net_iface_set_network_mask(nd, 0x800, mask);

			break;
		case SIOCADDRT:
			route = kmalloc(sizeof(struct route));
			memcpy(&mask, rt->gate.sa_data + 2, 4);
			route->gateway.address = mask;
			memcpy(&mask, rt->dest.sa_data + 2, 4);
			route->destination.address = mask;
			memcpy(&mask, rt->mask.sa_data + 2, 4);
			route->netmask = mask;
			route->interface = nd;
			route->flags = rt->flags;
			printk(0, "add route: %x %x %x %x %s\n", route->destination.address, route->gateway.address, route->netmask, route->flags, nd->name);
			net_route_add_entry(route);
			break;
		default:
			return -EINVAL;
	}
}

