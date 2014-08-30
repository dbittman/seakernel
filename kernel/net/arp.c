#include <sea/net/packet.h>
#include <sea/net/ethernet.h>
#include <sea/net/arp.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
struct hash_table *ipv4_hash = 0; /* TODO: hashes for any protocol */

static void arp_get_mac(uint8_t *mac, uint16_t m1, uint16_t m2, uint16_t m3)
{
	mac[0] = (m1 & 0xFF);
	mac[1] = ((m1>>8) & 0xFF);
	mac[2] = (m2 & 0xFF);
	mac[3] = ((m2>>8) & 0xFF);
	mac[4] = (m3 & 0xFF);
	mac[5] = ((m3>>8) & 0xFF);
}

static void arp_set_mac(uint8_t *mac, uint16_t *m1, uint16_t *m2, uint16_t *m3)
{
	*m1 = (mac[0] | (mac[1] << 8));
	*m2 = (mac[2] | (mac[3] << 8));
	*m3 = (mac[4] | (mac[5] << 8));
}

static void arp_add_entry_to_hash(struct hash_table *hash, uint16_t prot_addr[2], struct arp_entry *entry)
{
	void *tmp;
	/* we have to first lookup the address, as we may be changing an entry */
	if(hash_table_get_entry(hash, prot_addr, sizeof(uint16_t), 2, &tmp) != -ENOENT) {
		hash_table_delete_entry(hash, prot_addr, sizeof(uint16_t), 2);
		/* TODO: PROBLEM: this is not thread-safe */
		kfree(tmp);
	}
	printk(0, "[arp]: adding entry %x:%x\n", prot_addr[0], prot_addr[1]);
	hash_table_set_entry(hash, prot_addr, sizeof(uint16_t), 2, entry);
}

static struct hash_table *arp_create_cache()
{
	struct hash_table *hash = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(hash, HASH_RESIZE_MODE_IGNORE, 100);
	hash_table_specify_function(hash, HASH_FUNCTION_BYTE_SUM);
	return hash;
}

static void arp_send_packet(struct net_dev *nd, struct arp_packet *packet, int broadcast)
{
	struct ethernet_header eh;
	uint8_t dest[6];
	if(broadcast)
		memset(dest, 0xFF, 6);
	else
		arp_get_mac(dest, packet->tar_hw_addr_1, packet->tar_hw_addr_2, packet->tar_hw_addr_3);
	ethernet_construct_header(&eh, nd->mac, dest, ETHERTYPE_ARP);
	ethernet_send_packet(nd, &eh, (unsigned char *)packet, sizeof(*packet));
}

void arp_send_request(struct net_dev *nd, uint16_t prot_type, uint8_t prot_addr[4], int addr_len)
{
	struct arp_packet packet;
	packet.hw_type = HOST_TO_BIG16(1); /* ethernet */
	packet.p_type = HOST_TO_BIG16(prot_type);
	packet.hw_addr_len = 6;
	packet.p_addr_len = addr_len;
	packet.oper = HOST_TO_BIG16(ARP_OPER_REQUEST);
	
	arp_set_mac(nd->mac, &packet.src_hw_addr_1, &packet.src_hw_addr_2, &packet.src_hw_addr_3);
	uint8_t src_p[4];
	net_iface_get_prot_addr(nd, ETHERTYPE_IPV4, src_p);
	packet.src_p_addr_1 = src_p[0] | (src_p[1] << 8);
	packet.src_p_addr_2 = src_p[2] | (src_p[3] << 8);

	packet.tar_hw_addr_1 = packet.tar_hw_addr_2 = packet.tar_hw_addr_3 = 0;
	packet.tar_p_addr_1 = prot_addr[0] | (prot_addr[1] << 8);
	packet.tar_p_addr_2 = prot_addr[2] | (prot_addr[3] << 8);

	arp_send_packet(nd, &packet, 1);
}

static struct arp_entry *arp_do_lookup(int ethertype, uint16_t prot_addr[2])
{
	if(!ipv4_hash)
		return 0;
	void *entry;
	if(hash_table_get_entry(ipv4_hash, prot_addr, sizeof(uint16_t), 2, &entry) == -ENOENT)
		return 0;
	return entry;
}

void arp_remove_entry(int ptype, uint8_t paddr[4])
{
	/* a little bit manipulation */
	uint16_t pr[2];
	pr[0] = paddr[0] | (paddr[1] << 8);
	pr[1] = paddr[2] | (paddr[3] << 8);
	
	void *ent;
	if(hash_table_get_entry(ipv4_hash, pr, sizeof(uint16_t), 2, &ent) != -ENOENT) {
		hash_table_delete_entry(ipv4_hash, pr, sizeof(uint16_t), 2);
		kfree(ent);
	}
}

int arp_lookup(int ptype, uint8_t paddr[4], uint8_t hwaddr[6])
{
	/* a little bit manipulation */
	uint16_t pr[2];
	pr[0] = paddr[0] | (paddr[1] << 8);
	pr[1] = paddr[2] | (paddr[3] << 8);
	struct arp_entry *ent = arp_do_lookup(ptype, pr);
	if(!ent)
		return -ENOENT;
	/* TODO: timeout? */
	arp_get_mac(hwaddr, ent->hw_addr[0], ent->hw_addr[1], ent->hw_addr[2]);
	return 0;
}

static int __arp_compare_address(uint8_t addr[4], uint16_t a1, uint16_t a2)
{
	if((addr[0] | (addr[1] << 8)) == a1 && (addr[2] | (addr[3] << 8)) == a2)
		return 0;
	return 1;
}

int arp_receive_packet(struct net_dev *nd, struct arp_packet *packet)
{
	uint8_t mac[6];
	arp_get_mac(mac, packet->src_hw_addr_1, packet->src_hw_addr_2, packet->src_hw_addr_3);
	uint16_t oper = BIG_TO_HOST16(packet->oper);
	if(oper == ARP_OPER_REQUEST)
	{
		/* get this interface's protocol address */
		uint16_t ptype = BIG_TO_HOST16(packet->p_type);
		uint8_t ifaddr[4];
		net_iface_get_prot_addr(nd, ptype, ifaddr);
		if(!__arp_compare_address(ifaddr, packet->tar_p_addr_1, packet->tar_p_addr_2))
		{
			/* this is us! Form the reply packet */
			struct arp_packet reply;
			memcpy(&reply, packet, sizeof(*packet));
			reply.tar_hw_addr_1 = packet->src_hw_addr_1;
			reply.tar_hw_addr_2 = packet->src_hw_addr_2;
			reply.tar_hw_addr_3 = packet->src_hw_addr_3;
			
			arp_set_mac(nd->mac, 
					&(reply.src_hw_addr_1), &(reply.src_hw_addr_2), &(reply.src_hw_addr_3));
			
			reply.tar_p_addr_1 = packet->src_p_addr_1;
			reply.tar_p_addr_2 = packet->src_p_addr_2;
			reply.oper = HOST_TO_BIG16(ARP_OPER_REPLY);
			
			reply.src_p_addr_1 = ifaddr[0] | (ifaddr[1] << 8);
			reply.src_p_addr_2 = ifaddr[2] | (ifaddr[3] << 8);
			
			arp_send_packet(nd, &reply, 0);
		}
	}
	uint16_t p_addr[2];
	p_addr[0] = packet->src_p_addr_1;
	p_addr[1] = packet->src_p_addr_2;
	if((oper == ARP_OPER_REQUEST || oper == ARP_OPER_REPLY) 
			&& !arp_do_lookup(BIG_TO_HOST16(packet->p_type), p_addr)) {
		/* cache the ARP info from the source */
		struct arp_entry *entry = kmalloc(sizeof(struct arp_entry));
		entry->prot_addr[0] = p_addr[0];
		entry->prot_addr[1] = p_addr[1];
		entry->type = BIG_TO_HOST16(packet->p_type);
		entry->hw_addr[0] = packet->src_hw_addr_1;
		entry->hw_addr[1] = packet->src_hw_addr_2;
		entry->hw_addr[2] = packet->src_hw_addr_3;
		entry->hw_len = 6;
		entry->prot_len = 4;

		if(!ipv4_hash)
			ipv4_hash = arp_create_cache();
		arp_add_entry_to_hash(ipv4_hash, p_addr, entry);
	}
	return 0;
}

