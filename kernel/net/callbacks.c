#include <sea/kernel.h>
#include <sea/types.h>
#include <sea/net/net.h>
#include <sea/errno.h>

int net_callback_poll(struct net_dev *nd, struct net_packet *packets, int max)
{
	if(!nd || !nd->callbacks || !nd->callbacks->poll || !packets)
		return -EINVAL;
	return nd->callbacks->poll(nd, packets, max);
}

int net_callback_change_link(struct net_dev *nd, uint32_t link)
{
	if(!nd || !nd->callbacks || !nd->callbacks->change_link)
		return -EINVAL;
	return nd->callbacks->change_link(nd, link);
}

int net_callback_set_flags(struct net_dev *nd, uint32_t flags)
{
	if(!nd || !nd->callbacks || !nd->callbacks->set_flags)
		return -EINVAL;
	return nd->callbacks->set_flags(nd, flags);
}

int net_callback_get_flags(struct net_dev *nd, uint32_t *flags)
{
	if(!nd || !nd->callbacks || !nd->callbacks->get_flags || !flags)
		return -EINVAL;
	return nd->callbacks->get_flags(nd, flags);
}

int net_callback_send(struct net_dev *nd, struct net_packet *packets, int count)
{
	if(!nd || !nd->callbacks || !nd->callbacks->send || !packets)
		return -EINVAL;
	return nd->callbacks->send(nd, packets, count);
}

int net_callback_get_mac(struct net_dev *nd, uint8_t mac[6])
{
	if(!nd || !nd->callbacks || !nd->callbacks->get_mac || !mac)
		return -EINVAL;
	return nd->callbacks->get_mac(nd, mac);
}
