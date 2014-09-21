#ifndef __SEA_NET_INTERFACE_H
#define __SEA_NET_INTERFACE_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/kthread.h>
#include <sea/fs/inode.h>
#include <sea/fs/socket.h>
#include <sea/sys/ioctls.h>
#define IFACE_FLAG_UP           0x1
#define IFACE_FLAG_ACCBROADCAST 0x2
#define IFACE_FLAG_FORWARD      0x4
#define NET_HWTYPE_ETHERNET 1

#define IFNAMSIZ 16

struct net_dev {
	char name[IFNAMSIZ];
	struct inode *devnode;
	int num;
	int flags;
	uint32_t state;
	size_t rx_count, tx_count, rx_err_count, tx_err_count, rx_pending;
	int dropped;
	time_t rx_thread_lastwork;
	/* these fields are specified by the driver at time of net_dev creation */
	struct net_dev_calls *callbacks;
	void *data; /* driver specific data */

	uint8_t hw_address[6];
	uint8_t net_address[8];
	int net_address_len, hw_address_len;
	uint32_t netmask;
	uint16_t hw_type;
	int data_header_len;
	int mtu;

	struct llistnode *node;
	struct kthread rec_thread;
};

struct net_packet;

struct net_dev_calls {
	/* poll shall return received packets from the device in the array packets, up to
	 * the number specified by max. This call does not block, and will return no packets
	 * if none are available, and can return less than max packets if only some are
	 * available. 
	 */
	int (*poll)(struct net_dev *, struct net_packet *packets, int max);
	int (*send)(struct net_dev *, struct net_packet *packets, int count);
	int (*get_mac)(struct net_dev *, uint8_t mac[6]);
	int (*set_flags)(struct net_dev *, int);
	int (*change_link)(struct net_dev *, uint32_t);
};

int net_callback_poll(struct net_dev *, struct net_packet *, int);
int net_callback_change_link(struct net_dev *, uint32_t);
int net_callback_set_flags(struct net_dev *, int);
int net_callback_send(struct net_dev *nd, struct net_packet *packets, int count);
int net_callback_get_mac(struct net_dev *nd, uint8_t mac[6]);

struct net_dev *net_add_device(struct net_dev_calls *fn, void *);
int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count);

void net_iface_get_network_mask(struct net_dev *nd, int type, uint32_t *mask);
void net_iface_set_network_mask(struct net_dev *nd, int type, uint32_t mask);
void net_iface_set_network_addr(struct net_dev *nd, int type, uint8_t *addr);
void net_iface_get_network_addr(struct net_dev *nd, int type, uint8_t *addr);
int net_iface_set_flags(struct net_dev *nd, int flags);
int net_iface_get_flags(struct net_dev *nd);

struct  ifreq {
	char    ifr_name[IFNAMSIZ];             /* if name, e.g. "en0" */
	union {
		struct  sockaddr ifru_addr;
		struct  sockaddr ifru_dstaddr;
		struct  sockaddr ifru_broadaddr;
		struct  sockaddr ifru_netmask;
		short   ifru_flags[2];
		short   ifru_index;
		int     ifru_metric;
		int     ifru_mtu;
		int     ifru_phys;
		int     ifru_media;
		void *ifru_data;
		int     ifru_cap[2];
	} ifr_ifru;
#define ifr_addr        ifr_ifru.ifru_addr      /* address */
#define ifr_dstaddr     ifr_ifru.ifru_dstaddr   /* other end of p-to-p link */
#define ifr_broadaddr   ifr_ifru.ifru_broadaddr /* broadcast address */
#define ifr_netmask     ifr_ifru.ifru_netmask   /* interface net mask   */
#define ifr_flags       ifr_ifru.ifru_flags[0]  /* flags */
#define ifr_prevflags   ifr_ifru.ifru_flags[1]  /* flags */
#define ifr_metric      ifr_ifru.ifru_metric    /* metric */
#define ifr_mtu         ifr_ifru.ifru_mtu       /* mtu */
#define ifr_phys        ifr_ifru.ifru_phys      /* physical wire */
#define ifr_media       ifr_ifru.ifru_media     /* physical media */
#define ifr_data        ifr_ifru.ifru_data      /* for use by interface */
#define ifr_reqcap      ifr_ifru.ifru_cap[0]    /* requested capabilities */
#define ifr_curcap      ifr_ifru.ifru_cap[1]    /* current capabilities */
#define ifr_index       ifr_ifru.ifru_index     /* interface index */
};

#endif

