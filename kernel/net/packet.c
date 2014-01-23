#include <kernel.h>
#include <atomic.h>
#include <net.h>
#include <ll.h>
#include <task.h>
#include <symbol.h>
#include <asm/system.h>

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

struct __attribute__((__packed__)) eth_header {
	uint8_t dest_mac[6];
	uint8_t src_mac[6];
	uint16_t type;
};

struct __attribute__((__packed__)) arp_packet {
	uint16_t hw_type;
	uint16_t prot_type;
	uint8_t hwa_len;
	uint8_t prota_len;
	uint16_t oper;
	uint16_t shwa1;
	uint16_t shwa2;
	uint16_t shwa3;
	uint16_t spa1;
	uint16_t spa2;
	uint16_t thwa1;
	uint16_t thwa2;
	uint16_t thwa3;
	uint16_t tpa1;
	uint16_t tpa2;
};

void net_receive_packet(struct net_dev *nd, struct net_packet *packets, int count)
{
	/* all the packets sent to this function must be copied */
	kprintf("NET: PACKET: %d\n", packets[0].length);
	struct eth_header *e = (struct eth_header *)packets[0].data;
	if(BIG_TO_HOST16(e->type) == 0x806)
	{
		kprintf("ARP packet!\n");
		struct arp_packet *a = (struct arp_packet *)(packets[0].data + sizeof(struct eth_header));
		
		if(BIG_TO_HOST16(a->prot_type) == 0x800) {
			kprintf("ARP IPv4: from %d.%d.%d.%d, ", (uint8_t)a->spa1, (uint8_t)(a->spa1>>8), (uint8_t)a->spa2, (uint8_t)(a->spa2>>8));
			if(BIG_TO_HOST16(a->oper) == 1)
				kprintf("who is ");
			kprintf("%d.%d.%d.%d ?\n", (uint8_t)a->tpa1, (uint8_t)(a->tpa1>>8), (uint8_t)a->tpa2, (uint8_t)(a->tpa2>>8));
			
		}
		
	}
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
				schedule();
		} else
			schedule();
	} while(!(ret=net_callback_poll(nd, packets, max)));
	return ret;
}
