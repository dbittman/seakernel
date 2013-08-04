#include <kernel.h>
#include <dev.h>
#include <mod.h>
#include <modules/crc32.h>
intptr_t find_kernel_function(char * unres);
int (*process_packet_ipv4)(char *, int);
int (*process_packet_ipv6)(char *, int);
#define IPV4 0xabcdef /* Whatever */
int process_ethernet_packet(char *buffer, unsigned length)
{
	kprintf("[eth]: Process packet: %x %d\n", buffer, length);
	int type=0;
	if(type == IPV4 && process_packet_ipv4)
		process_packet_ipv4(buffer, length);
	return 0;
}

void reload_eth_routing_table()
{
	process_packet_ipv4 = (int (*)(char *, int))find_kernel_function("process_packet_ipv4");
	process_packet_ipv6 = (int (*)(char *, int))find_kernel_function("process_packet_ipv6");
	printk(0, "[ethernet]: Reloaded table and found ipv4=%x, ipv6=%x\n", process_packet_ipv4, process_packet_ipv6);
}

int module_install()
{
	add_kernel_symbol(process_ethernet_packet);
	add_kernel_symbol(reload_eth_routing_table);
	reload_eth_routing_table();
	return 0;
}

int module_exit()
{
	remove_kernel_symbol("process_ethernet_packet");
	remove_kernel_symbol("reload_eth_routing_table");
	
	return 0;
}

int module_deps()
{
	return KVERSION;
}
