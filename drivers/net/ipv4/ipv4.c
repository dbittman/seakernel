#include <sea/net/packet.h>
#include <sea/net/interface.h>
#include <sea/net/nlayer.h>
#include <sea/net/route.h>
#include <sea/net/datalayer.h>
#include <sea/net/ethertype.h>
#include <sea/net/arp.h>
#include <sea/net/tlayer.h>

#include <sea/tm/kthread.h>
#include <sea/tm/process.h>
#include <sea/tm/timing.h>

#include <sea/cpu/time.h>


#include <sea/mm/kmalloc.h>
#include <sea/lib/queue.h>

#include <sea/lib/linkedlist.h>
#include <sea/errno.h>
#include <sea/vsprintf.h>

#include <limits.h>

#include <modules/ipv4/ipv4sock.h>
#include <modules/ipv4/ipv4.h>
#include <modules/ipv4/icmp.h>

uint16_t ipv4_calc_checksum(void *__data, int length)
{
	uint8_t *data = __data;
	uint32_t sum = 0xFFFF;
	int i;
	for(i=0;i+1<length;i+=2) {
		sum += BIG_TO_HOST16(*(uint16_t *)(data + i));
		if(sum > 0xFFFF)
			sum -= 0xFFFF;
	}
	return HOST_TO_BIG16(~sum);
}

struct nlayer_protocol ipv4 = {
	.flags = 0,
	.receive = ipv4_receive_packet,
	.send = ipv4_enqueue_sockaddr,
};

int module_install(void)
{
	ipv4_tx_queue = queue_create(0, 0);
	frag_list = linkedlist_create(0, LINKEDLIST_MUTEX);
	ipv4_send_thread = kthread_create(0, "[kipv4-send]", 0, ipv4_sending_thread, 0);
	ipv4_send_thread->thread->priority = 100;
	net_nlayer_register_protocol(PF_INET, &ipv4);
	socket_set_calls(1 /* TODO */, &socket_calls_rawipv4);
	return 0;
}

int module_exit(void)
{
	/* shutdown the ipv4 sending thread */
	kthread_join(ipv4_send_thread, KT_JOIN_NONBLOCK);
	/* unregister everything */
	net_nlayer_unregister_protocol(PF_INET);
	socket_set_calls(1, 0);
	/* clean up the sending queue */
	while(queue_dequeue(ipv4_tx_queue));
	queue_destroy(ipv4_tx_queue);
	/* clean up the fragmentation resources */
	while(__ipv4_cleanup_fragments(1));
	linkedlist_destroy(frag_list);
	return 0;
}

