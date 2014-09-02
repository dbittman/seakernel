#ifndef __SEA_NET_INTERFACE_H
#define __SEA_NET_INTERFACE_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/kthread.h>

#define IFACE_FLAG_UP        0x1
#define IFACE_FLAG_BROADCAST 0x2

struct net_dev {
	int flags;
	uint32_t state;
	size_t rx_count, tx_count, rx_err_count, tx_err_count, rx_pending;
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

void net_iface_set_network_addr(struct net_dev *nd, int type, uint8_t *addr);
void net_iface_get_network_addr(struct net_dev *nd, int type, uint8_t *addr);
int net_iface_set_flags(struct net_dev *nd, int flags);
int net_iface_get_flags(struct net_dev *nd);

#endif

