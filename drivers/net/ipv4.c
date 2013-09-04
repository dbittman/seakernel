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
	add_kernel_symbol(process_packet_ipv4);
	reload_eth_routing_table();
	return 0;
}

int module_exit()
{
	remove_kernel_symbol("process_packet_ipv4");
	reload_eth_routing_table();
	return 0;
}

int module_deps(char *b)
{
	write_deps(b, "ethernet,:");
	return KVERSION;
}
