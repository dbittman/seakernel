#include <sea/net/packet.h>
#include <sea/net/arp.h>
#include <sea/asm/system.h>
#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/lib/hash.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/cpu/time.h>
#include <sea/net/ethertype.h>
#include <sea/net/datalayer.h>
#include <sea/tm/schedule.h>
#include <sea/mutex.h>

static struct hash_table *ipv4_hash = 0; /* TODO: hashes for any protocol */
static struct llist *outstanding = 0;
mutex_t *outlock, *hashlock;
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

static void arp_write_short(uint8_t bytes[4], uint16_t *a, uint16_t *b)
{
	*a = bytes[0] | (bytes[1] << 8);
	*b = bytes[2] | (bytes[3] << 8);
}

static int __arp_compare_address(uint8_t addr[4], uint16_t a1, uint16_t a2)
{
	if((addr[0] | (addr[1] << 8)) == a1 && (addr[2] | (addr[3] << 8)) == a2)
		return 0;
	return 1;
}

static struct arp_entry *__arp_get_outstanding_requests_entry(int prot_type, uint16_t addr[2], int check_time, int remove)
{
	mutex_acquire(outlock);
	struct llistnode *node;
	struct arp_entry *entry;
	ll_for_each_entry(outstanding, node, struct arp_entry *, entry) {
		if(check_time && (tm_get_ticks() > (entry->timestamp + TICKS_SECONDS(1)))) {
			ll_remove(outstanding, entry->node);
			kfree(entry);
			continue;
		}
		if(entry->type == prot_type && !memcmp(entry->prot_addr, addr, 2 * sizeof(uint16_t))) {
			/* found valid */
			if(remove) {
				ll_remove(outstanding, entry->node);
				kfree(entry);
				continue;
			}
			mutex_release(outlock);
			return entry;
		}
	}
	mutex_release(outlock);
	return 0;
}

static int arp_check_outstanding_requests(int prot_type, uint16_t addr[2])
{
	if(__arp_get_outstanding_requests_entry(prot_type, addr, 1, 0))
		return 1;
	return 0;
}

static void arp_remove_outstanding_requests(int prot_type, uint16_t addr[2])
{
	__arp_get_outstanding_requests_entry(prot_type, addr, 0, 1);
}

static void arp_add_outstanding_requests(int prot_type, uint16_t addr[2])
{
	struct arp_entry *entry = kmalloc(sizeof(struct arp_entry));
	entry->type = prot_type;
	memcpy(entry->prot_addr, addr, 2 * sizeof(uint16_t));
	mutex_acquire(outlock);
	entry->timestamp = tm_get_ticks();
	entry->node = ll_insert(outstanding, entry);
	mutex_release(outlock);
}

static void arp_add_entry_to_hash(struct hash_table *hash, uint16_t prot_addr[2], struct arp_entry *entry)
{
	void *tmp;
	/* we have to first lookup the address, as we may be changing an entry */
	mutex_acquire(hashlock);
	if(hash_table_get_entry(hash, prot_addr, sizeof(uint16_t), 2, &tmp) != -ENOENT) {
		hash_table_delete_entry(hash, prot_addr, sizeof(uint16_t), 2);
		kfree(tmp);
	}
	TRACE(0, "[arp]: adding entry %x:%x -- %x\n", prot_addr[0], prot_addr[1], entry->hw_addr[0]);
	hash_table_set_entry(hash, prot_addr, sizeof(uint16_t), 2, entry);
	mutex_release(hashlock);
}

static struct hash_table *arp_create_cache()
{
	struct hash_table *hash = hash_table_create(0, 0, HASH_TYPE_CHAIN);
	hash_table_resize(hash, HASH_RESIZE_MODE_IGNORE, 100);
	hash_table_specify_function(hash, HASH_FUNCTION_BYTE_SUM);
	return hash;
}

static void arp_send_packet(struct net_dev *nd, struct net_packet *netpacket, 
		struct arp_packet *packet, int broadcast)
{
	uint8_t dest[6];
	if(broadcast)
		memset(dest, 0xFF, 6);
	else
		arp_get_mac(dest, packet->tar_hw_addr_1, packet->tar_hw_addr_2, packet->tar_hw_addr_3);
	net_data_send(nd, netpacket, ETHERTYPE_ARP, dest, sizeof(*packet));
}

void arp_send_request(struct net_dev *nd, uint16_t prot_type, uint8_t prot_addr[4], int addr_len)
{
	/* first, we check for an outstanding current request to this address for this protocol.
	 * if it exists, we just return. If not, we record the request, and send it.
	 * NOTE: if we find a request that is over 1 second old, we re-send the request
	 * and reset the timeout value for it.
	 */
	struct arp_packet packet;
	//packet.tar_p_addr_1 = prot_addr[3] | (prot_addr[2] << 8);
	//packet.tar_p_addr_2 = prot_addr[1] | (prot_addr[0] << 8);
	arp_write_short(prot_addr, &packet.tar_p_addr_1, &packet.tar_p_addr_2);
	uint16_t addr[2];
	addr[0] = packet.tar_p_addr_1;
	addr[1] = packet.tar_p_addr_2;
	if(arp_check_outstanding_requests(prot_type, addr))
		return;

	packet.hw_type = HOST_TO_BIG16(nd->hw_type);
	packet.p_type = HOST_TO_BIG16(prot_type);
	packet.hw_addr_len = 6;
	packet.p_addr_len = addr_len;
	packet.oper = HOST_TO_BIG16(ARP_OPER_REQUEST);
	
	arp_set_mac(nd->hw_address, &packet.src_hw_addr_1, &packet.src_hw_addr_2, &packet.src_hw_addr_3);
	uint8_t src_p[4];
	net_iface_get_network_addr(nd, prot_type, src_p);
	//packet.src_p_addr_1 = src_p[3] | (src_p[2] << 8);
	//packet.src_p_addr_2 = src_p[1] | (src_p[0] << 8);
	arp_write_short(src_p, &packet.src_p_addr_1, &packet.src_p_addr_2);

	packet.tar_hw_addr_1 = packet.tar_hw_addr_2 = packet.tar_hw_addr_3 = 0;

	struct net_packet netpacket;
	net_packet_create(&netpacket, 0);
	netpacket.data_header = (void *)netpacket.data;
	
	memcpy((void *)((addr_t)netpacket.data + nd->data_header_len), &packet, sizeof(packet));
	
	arp_send_packet(nd, &netpacket, &packet, 1);
	net_packet_put(&netpacket, NP_FLAG_DESTROY);
	arp_add_outstanding_requests(prot_type, addr);
}

static struct arp_entry *arp_do_lookup(int ethertype, uint16_t prot_addr[2])
{
	if(!ipv4_hash)
		return 0;
	void *entry;
	mutex_acquire(hashlock);
	if(hash_table_get_entry(ipv4_hash, prot_addr, sizeof(uint16_t), 2, &entry) == -ENOENT) {
		mutex_release(hashlock);
		return 0;
	}
	mutex_release(hashlock);
	return entry;
}

void arp_remove_entry(int ptype, uint8_t paddr[4])
{
	/* a little bit manipulation */
	uint16_t pr[2];
	//pr[0] = paddr[3] | (paddr[2] << 8);
	//pr[1] = paddr[1] | (paddr[0] << 8);
	arp_write_short(paddr, &pr[0], &pr[1]);
	
	void *ent;
	mutex_acquire(hashlock);
	if(hash_table_get_entry(ipv4_hash, pr, sizeof(uint16_t), 2, &ent) != -ENOENT) {
		hash_table_delete_entry(ipv4_hash, pr, sizeof(uint16_t), 2);
		kfree(ent);
	}
	mutex_release(hashlock);
}

int arp_lookup(int ptype, uint8_t paddr[4], uint8_t hwaddr[6])
{
	/* a little bit manipulation */
	uint16_t pr[2];
//	pr[0] = paddr[3] | (paddr[2] << 8);
//	pr[1] = paddr[1] | (paddr[0] << 8);
	arp_write_short(paddr, &pr[0], &pr[1]);
	struct arp_entry *ent = arp_do_lookup(ptype, pr);
	if(!ent)
		return -ENOENT;
	arp_get_mac(hwaddr, ent->hw_addr[0], ent->hw_addr[1], ent->hw_addr[2]);
	return 0;
}

int arp_receive_packet(struct net_dev *nd, struct net_packet *netpacket, struct arp_packet *packet)
{
	uint8_t mac[6];
	arp_get_mac(mac, packet->src_hw_addr_1, packet->src_hw_addr_2, packet->src_hw_addr_3);
	uint16_t oper = BIG_TO_HOST16(packet->oper);
	if(oper == ARP_OPER_REQUEST)
	{
		/* get this interface's protocol address */
		uint16_t ptype = BIG_TO_HOST16(packet->p_type);
		uint8_t ifaddr[4];
		net_iface_get_network_addr(nd, ptype, ifaddr);
		if(!__arp_compare_address(ifaddr, packet->tar_p_addr_1, packet->tar_p_addr_2))
		{
			/* this is us! Form the reply packet */
			packet->tar_hw_addr_1 = packet->src_hw_addr_1;
			packet->tar_hw_addr_2 = packet->src_hw_addr_2;
			packet->tar_hw_addr_3 = packet->src_hw_addr_3;
			
			arp_set_mac(nd->hw_address, 
					&(packet->src_hw_addr_1), &(packet->src_hw_addr_2), &(packet->src_hw_addr_3));
			
			packet->tar_p_addr_1 = packet->src_p_addr_1;
			packet->tar_p_addr_2 = packet->src_p_addr_2;
			packet->oper = HOST_TO_BIG16(ARP_OPER_REPLY);
			
			//packet->src_p_addr_1 = ifaddr[3] | (ifaddr[2] << 8);
			//packet->src_p_addr_2 = ifaddr[1] | (ifaddr[0] << 8);
			arp_write_short(ifaddr, &packet->src_p_addr_1, &packet->src_p_addr_2);
			
			arp_send_packet(nd, netpacket, packet, 0);
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
	if(oper == ARP_OPER_REQUEST || oper == ARP_OPER_REPLY) 
		arp_remove_outstanding_requests(BIG_TO_HOST16(packet->p_type), p_addr);
	return 0;
}

void arp_init()
{
	outstanding = ll_create_lockless(0);
	outlock = mutex_create(0, 0);
	hashlock = mutex_create(0, 0);
}

