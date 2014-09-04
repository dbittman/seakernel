#include <sea/fs/socket.h>
#include <sea/net/ipv4sock.h>
#include <sea/net/ipv4.h>

struct socket_calls socket_calls_rawipv4 = {
	.init = 0,
	.accept = 0,
	.listen = 0,
	.connect = 0,
	.bind = 0,
	.shutdown = 0,
	.destroy = 0,
	.recv = 0,
	.send = 0
};

