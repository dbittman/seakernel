#include <sea/net/interface.h>
#include <sea/net/ipv4.h>
#include <sea/net/udp.h>
#include <sea/net/packet.h>

#include <sea/fs/socket.h>

#define GET_PORT(sa) (BIG_TO_HOST16((*(uint16_t *)sa->sa_data)))

static int recvfrom(struct socket *, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len);
static int sendto(struct socket *, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len);
static int shutdown(struct socket *sock, int how);
static int bind(struct socket *, const struct sockaddr *addr, socklen_t len);

static struct socket *ports[MAX_PORTS];

struct socket_calls socket_calls_udp = {
	.init = 0,
	.accept = 0,
	.listen = 0,
	.connect = 0,
	.bind = 0,
	.shutdown = shutdown,
	.destroy = 0,
	.recvfrom = recvfrom,
	.sendto = sendto,
	.select = 0
};

static void udp_init()
{
	static int done = 0;
	if(!done) {
		memset(ports, 0, sizeof(ports));
		done = 0;
	}
}

static int get_ephemeral_port()
{
	
}

static int bind(struct socket *sock, const struct sockaddr *addr, socklen_t len)
{
	udp_init();
	int port = GET_PORT(addr);
	
}

static int recvfrom(struct socket *sock, void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t *addr_len)
{
	udp_init();

}

static int sendto(struct socket *sock, const void *buffer, size_t length,
		int flags, struct sockaddr *addr, socklen_t addr_len)
{
	udp_init();

}

static int shutdown(struct socket *sock, int how)
{
	udp_init();

}

