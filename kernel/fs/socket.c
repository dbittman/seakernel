#include <sea/fs/socket.h>
#include <sea/fs/file.h>
#include <sea/errno.h>

static struct socket *create_socket(int *errcode)
{

}

int sys_socket(int domain, int type, int prot)
{
	if(type > SOCK_MAXTYPE || type <= 0)
		return -EAFNOSUPPORT;
	if(!prot)
		prot = __socket_default_protocols_per_type[type];
	if(prot > PROT_MAXPROT || prot <= 0)
		return -EPROTONOSUPPORT;
	if(domain > PF_MAX || domain <= 0)
		return -EAFNOSUPPORT;
	int err;
	struct socket *sock = create_socket(&err);
	if(!sock)
		return err;
	sock->domain = domain;
	sock->type = type;
	sock->prot = prot;

	return sock->fd;
}

int sys_connect(int socket, const struct sockaddr *addr, socklen_t len);
int sys_accept(int socket, struct sockaddr *restrict addr, socklen_t *restrict addr_len);
int sys_listen(int socket, int backlog);
int sys_bind(int socket, const struct sockaddr *address, socklen_t address_len);
int sys_getsockopt(int socket, int level, int option_name,
		void *restrict option_value, socklen_t *restrict option_len);
int sys_setsockopt(int socket, int level, int option_name,
		const void *option_value, socklen_t option_len);
int sys_shutdown(int socket, int how);
ssize_t sys_recv(int socket, void *buffer, size_t length, int flags);
ssize_t sys_send(int socket, const void *buffer, size_t length, int flags);


