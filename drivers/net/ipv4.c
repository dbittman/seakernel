#include <sea/kernel.h>
#include <sea/loader/symbol.h>
#include <sea/loader/module.h>
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

int module_exit()
{
	loader_remove_kernel_symbol("process_packet_ipv4");
	reload_eth_routing_table();
	return 0;
}
