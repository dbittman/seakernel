#include <sea/net/interface.h>
#include <sea/net/packet.h>
#include <sea/net/datalayer.h>
#include <sea/net/ethertype.h>
#include <sea/asm/system.h>
#define MAX_LO 32

struct lodevice {
	struct net_dev *nd;
};

static struct lodevice los[MAX_LO];
static int send(struct net_dev *nd, struct net_packet *packets, int count);
struct net_dev_calls lo_calls = {
	.poll = 0,
	.send = send,
	.get_mac = 0,
	.set_flags = 0,
	.change_link = 0
};

static int send(struct net_dev *nd, struct net_packet *packets, int count)
{
	if(!(nd->flags & IFACE_FLAG_UP))
		return 0;
	/* immediately just receive the packets... */
	net_receive_packet(nd, packets, count);
	return count;
}

int loop_convert_sa_family(sa_family_t sa)
{
	switch(sa) {
		case AF_INET:
			return ETHERTYPE_IPV4;
		case AF_ARP:
			return ETHERTYPE_ARP;
	}
	return -1;
}

static void data_layer_send(struct net_dev *nd, struct net_packet *packet, sa_family_t sa, uint8_t dest[6], int len)
{
	int etype = loop_convert_sa_family(sa);
	*(uint16_t *)(packet->data) = HOST_TO_BIG16(etype);
	net_transmit_packet(nd, packet, 1);
}

struct data_layer_protocol loop_prot = {
	.flags = 0,
	.send = data_layer_send,
	.receive = 0,
};

void net_lo_create()
{
	for(int i=0;i<MAX_LO;i++) {
		if(!los[i].nd) {
			struct net_dev *nd = los[i].nd = net_add_device(&lo_calls, &los[i]);
			nd->data_header_len = 2;
			nd->hw_address_len = 0;
			nd->hw_type = NET_HWTYPE_LOOP;
			nd->brate = 100000000; /* TODO */
			return;
		}
	}
}

int module_install()
{
	for(int i=0;i<MAX_LO;i++)
		los[i].nd = 0;
	net_lo_create();
	net_data_register_protocol(NET_HWTYPE_LOOP, &loop_prot);
	return 0;
}

int module_exit()
{
	/* TODO: shutdown interfaces */
	return 0;
}

