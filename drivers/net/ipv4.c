#include <kernel.h>
#include <symbol.h>
#include <module.h>
void reload_eth_routing_table();
int process_packet_ipv4(char *b, unsigned len)
{
	return 0;
}

int module_install()
{
	loader_add_kernel_symbol(process_packet_ipv4);
	reload_eth_routing_table();
	return 0;
}

int module_tm_exit()
{
	loader_remove_kernel_symbol("process_packet_ipv4");
	reload_eth_routing_table();
	return 0;
}
