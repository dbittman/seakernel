#include <sea/net/net.h>
#include <sea/net/ethernet.h>
#include <sea/net/arp.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
void arp_get_mac(uint8_t *mac, uint16_t m1, uint16_t m2, uint16_t m3)
{
	mac[0] = (m1 & 0xFF);
	mac[1] = ((m1>>8) & 0xFF);
	mac[2] = (m2 & 0xFF);
	mac[3] = ((m2>>8) & 0xFF);
	mac[4] = (m3 & 0xFF);
	mac[5] = ((m3>>8) & 0xFF);
}

void arp_set_mac(uint8_t *mac, uint16_t *m1, uint16_t *m2, uint16_t *m3)
{
	*m1 = (mac[0] | (mac[1] << 8));
	*m2 = (mac[2] | (mac[3] << 8));
	*m3 = (mac[4] | (mac[5] << 8));
}

void arp_send_packet(struct net_dev *nd, struct arp_packet *packet)
{
	struct ethernet_header eh;
	uint8_t dest[6];
	arp_get_mac(dest, packet->tar_hw_addr_1, packet->tar_hw_addr_2, packet->tar_hw_addr_3);
	ethernet_construct_header(&eh, nd->mac, dest, ETHERTYPE_ARP);
	ethernet_send_packet(nd, &eh, (unsigned char *)packet, sizeof(*packet));
}

int arp_receive_packet(struct net_dev *nd, struct arp_packet *packet)
{
	uint8_t mac[6];
	arp_get_mac(mac, packet->src_hw_addr_1, packet->src_hw_addr_2, packet->src_hw_addr_3);
	kprintf("ARP: from mac: ");
	for(int i=0;i<6;i++)
		kprintf("%x:", mac[i]);
	kprintf("\n");
	
	kprintf("%x %x\n", packet->src_p_addr_1, packet->src_p_addr_2);
	
	if(BIG_TO_HOST16(packet->oper) == ARP_OPER_REQUEST)
	{
		if(1/* protocol address equals our address */)
		{
			struct arp_packet reply;
			memcpy(&reply, packet, sizeof(*packet));
			reply.tar_hw_addr_1 = packet->src_hw_addr_1;
			reply.tar_hw_addr_2 = packet->src_hw_addr_2;
			reply.tar_hw_addr_3 = packet->src_hw_addr_3;
			
			arp_set_mac(nd->mac, &(reply.src_hw_addr_1), &(reply.src_hw_addr_2), &(reply.src_hw_addr_3));
			
			reply.tar_p_addr_1 = packet->src_p_addr_1;
			reply.tar_p_addr_2 = packet->src_p_addr_2;
			reply.oper = HOST_TO_BIG16(ARP_OPER_REPLY);
			
			reply.src_p_addr_1 = 0xa;
			reply.src_p_addr_2 = 0x200;
			
			arp_send_packet(nd, &reply);
		}
	}
	return 0;
}
