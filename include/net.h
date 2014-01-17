#ifndef __SEA_NET_H
#define __SEA_NET_H

#include <types.h>

struct net_dev_calls {
	int (*poll)(int);
	int (*set_flags)(int);
	int (*get_flags)(int);
	int (*change_link)(int);
};

struct net_dev {
	uint32_t flags;
	uint32_t state;
	size_t rx_count, tx_count, rx_err_count, tx_err_count;
	struct net_dev_calls *callbacks;
};




void net_notify_packet_ready(struct net_dev *nd);

#endif
