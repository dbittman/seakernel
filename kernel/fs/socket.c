#include <sea/fs/socket.h>
#include <sea/fs/file.h>
#include <sea/fs/inode.h>
#include <sea/errno.h>
#include <sea/mm/kmalloc.h>
#include <sea/vsprintf.h>
#include <sea/net/ipv4sock.h>
#include <sea/net/data_queue.h>
#include <sea/tm/schedule.h>
#include <sea/fs/fcntl.h>
#include <sea/dm/dev.h>
/* network worker threads just add the packet to the
 * queues that read from them (can be multiple queues)
 * and give proper refcounts. user programs are then woken
 * up and read from the queues.
 *
 * When writing, the user program creates the packet and sends
 * it all the way down to the network layer (where, in ipv4 at least,
 * it is taken over by a sending thread).
 */

struct socket_calls socket_calls_null = {0,0,0,0,0,0,0,0,0,0};

struct socket_calls *__socket_calls_list[PROT_MAXPROT + 1] = {
	&socket_calls_null,
	&socket_calls_rawipv4,
};

struct socket *socket_create(int *errcode)
{
	*errcode = 0;
	struct inode *inode = kmalloc(sizeof(struct inode));
	struct file *f = kmalloc(sizeof(struct file));
	f->inode = inode;
	inode->count = 1;
	inode->f_count = 1;
	inode->mode |= S_IFSOCK;
	rwlock_create(&inode->rwl);
	f->count = 1;
	int fd = fs_add_file_pointer(current_task, f);
	if(fd < 0)
		*errcode = -ENFILE;
	struct socket *sock = kmalloc(sizeof(struct socket));
	sock->inode = inode;
	sock->file = f;
	sock->fd = fd;
	queue_create(&sock->rec_data_queue, 0);
	f->socket = sock;
	fs_fput(current_task, fd, 0);
	return sock;
}

static struct socket *get_socket(int fd, int *err)
{
	*err = 0;
	struct file *f = fs_get_file_pointer(current_task, fd);
	if(!f) {
		*err = -EBADF;
		return 0;
	}
	fs_fput(current_task, fd, 0);
	if(!f->socket) {
		*err = -ENOTSOCK;
		return 0;
	}
	return f->socket;
}

static void socket_destroy(struct socket *sock)
{
	if(sock->calls->destroy)
		sock->calls->destroy(sock);
	struct file *file = sock->file;
	file->socket = 0;
	sys_close(sock->fd);
	queue_destroy(&sock->rec_data_queue);
	kfree(sock);
}

int socket_select(struct file *f, int rw)
{
	if(!f->socket)
		return -ENOTSOCK;
	if(rw == READ && !(f->socket->flags & SOCK_FLAG_ALLOWRECV))
		return 0;
	if(rw == WRITE && !(f->socket->flags & SOCK_FLAG_ALLOWSEND))
		return 0;
	int r = 1;
	if(f->socket->calls->select)
		r = f->socket->calls->select(f->socket, rw);
	if(r == 0)
		return 0;
	/* if the protocol says that we're allowed to read or write, that might
	 * not be true for the socket layer...check the data queue */
	if(rw == READ)
		return f->socket->rec_data_queue.count > 0;
	return 1;
}

static struct socket_calls *socket_get_calls(int prot)
{
	return __socket_calls_list[prot];
}

void socket_set_calls(int prot, struct socket_calls *calls)
{
	__socket_calls_list[prot] = calls;
}

int sys_socket(int domain, int type, int prot)
{
	if(type > SOCK_MAXTYPE || type <= 0)
		return -EAFNOSUPPORT;
	if(!prot)
		prot = __socket_default_protocols_per_type[type];
	struct socket_calls *calls = socket_get_calls(prot);
	if(prot > PROT_MAXPROT || prot <= 0 || !calls)
		return -EPROTONOSUPPORT;
	if(domain > PF_MAX || domain <= 0)
		return -EAFNOSUPPORT;
	int err;
	struct socket *sock = socket_create(&err);
	if(!sock)
		return err;
	TRACE(0, "[socket]: created socket %d, d=%d, t=%d, p=%d\n", sock->fd, domain, type, prot);
	sock->domain = domain;
	sock->type = type;
	sock->prot = prot;
	sock->flags = SOCK_FLAG_ALLOWSEND | SOCK_FLAG_ALLOWRECV;
	sock->calls = calls;
	if(sock->calls->init)
		sock->calls->init(sock);
	return sock->fd;
}

int sys_connect(int socket, const struct sockaddr *addr, socklen_t len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	if(sock->flags & SOCK_FLAG_CONNECTING)
		return -EALREADY;
	if(sock->flags & SOCK_FLAG_CONNECTED)
		return -EISCONN;
	if(sock->sopt & SO_ACCEPTCONN) /* listening */
		return -EOPNOTSUPP;
	sock->flags |= SOCK_FLAG_CONNECTING;
	/* okay, tell the protocol to make the connection */
	TRACE(0, "[socket]: connecting %d\n", socket);
	int ret = -EOPNOTSUPP;
	if(sock->calls->connect)
		ret = sock->calls->connect(sock, addr, len);
	sock->flags &= ~SOCK_FLAG_CONNECTING;
	if(ret < 0)
		return ret;
	memcpy(&sock->peer, addr, len);
	sock->peer_len = len;
	sock->flags |= SOCK_FLAG_CONNECTED;
	return 0;
}

int sys_accept(int socket, struct sockaddr *restrict addr, socklen_t *restrict addr_len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	if(!(sock->flags & SOCK_FLAG_CONNECTED) || !(sock->sopt & SO_ACCEPTCONN))
		return -EINVAL;
	err = -EOPNOTSUPP;
	struct socket *sret = 0;
	TRACE(0, "[socket]: %d accepting\n", socket);
	if(sock->calls->accept)
		sret = sock->calls->accept(sock, addr, addr_len, &err);
	if(!sret)
		return err;
	return sret->fd;
}

int sys_listen(int socket, int backlog)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	int ret = -EOPNOTSUPP;
	if(sock->calls->listen)
		ret = sock->calls->listen(sock, backlog);
	if(ret < 0)
		return ret;
	sock->sopt |= SO_ACCEPTCONN;
	return 0;
}

void socket_bind(struct socket *sock, const struct sockaddr *address, socklen_t len)
{
	memcpy(&sock->local, address, len);
	sock->local_len = len;
	sock->flags |= SOCK_FLAG_BOUND;
}

int sys_bind(int socket, const struct sockaddr *address, socklen_t address_len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	int ret = -EOPNOTSUPP;
	TRACE(0, "[socket]: %d binding\n", socket);
	if(sock->calls->bind)
		ret = sock->calls->bind(sock, address, address_len);
	if(ret < 0)
		return ret;
	socket_bind(sock, address, address_len);
	return 0;
}

int sys_getsockopt(int socket, int level, int option_name,
		void *restrict option_value, socklen_t *restrict option_len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	uint64_t value;
	int size;
	if(level == SOL_SOCKET && option_name <= 0x1000) {
		value = sock->sopt & option_name;
		size = sizeof(int);
	} else if(level == SOL_SOCKET) {
		value = sock->sopt_extra[option_name - 0x1000];
		size = sock->sopt_extra_sizes[option_name - 0x1000];
	} else {
		value = sock->sopt_levels[level][option_name];
		size = sock->sopt_levels_sizes[level][option_name];
	}
	switch(size) {
		case 1:
			*(uint8_t *)option_value = (uint8_t)value;
			*option_len = size;
			break;
		case 2:
			*(uint16_t *)option_value = (uint16_t)value;
			*option_len = size;
			break;
		default: case 4:
			*(uint32_t *)option_value = (uint32_t)value;
			*option_len = size;
			break;
	}
	return 0;
}

/* TODO: SO_BINDTODEVICE */
int sys_setsockopt(int socket, int level, int option_name,
		const void *option_value, socklen_t option_len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	uint64_t value = 0;
	switch(option_len) {
		case 1:
			value = *(uint8_t *)option_value;
			break;
		case 2:
			value = *(uint16_t *)option_value;
			break;
		defualt: case 4:
			value = *(uint32_t *)option_value;
			break;
		case 8:
			value = *(uint64_t *)option_value;
			break;
	}
	TRACE(0, "[socket]: setsockopt(): level=%d, opt=%d, val=%x\n", level, option_name, value);
	if(level == SOL_SOCKET && option_name <= 0x1000) {
		if(value)
			sock->sopt |= option_name;
		else
			sock->sopt &= ~option_name;
	} else if(level == SOL_SOCKET) {
		sock->sopt_extra[option_name - 0x1000] = value;
		sock->sopt_extra_sizes[option_name - 0x1000] = option_len;
	} else {
		sock->sopt_levels[level][option_name] = value;
		sock->sopt_levels_sizes[level][option_name] = option_len;
	}
	return 0;
}

int sys_sockshutdown(int socket, int how)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	int rd = (how == SHUT_RD || how == SHUT_RDWR);
	int wr = (how == SHUT_WR || how == SHUT_RDWR);
	if(rd)
		sock->flags &= ~SOCK_FLAG_ALLOWRECV;
	if(wr)
		sock->flags &= ~SOCK_FLAG_ALLOWSEND;
	if(sock->calls->shutdown)
		sock->calls->shutdown(sock, how);
	if(!(sock->flags & SOCK_FLAG_ALLOWSEND) && !(sock->flags & SOCK_FLAG_ALLOWRECV))
		socket_destroy(sock);
	return 0;
}

ssize_t sys_recv(int socket, void *buffer, size_t length, int flags)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	if(!(sock->flags & SOCK_FLAG_ALLOWRECV))
		return -EIO;
	int ret = 0;
	if(sock->calls->recvfrom)
		ret = sock->calls->recvfrom(sock, buffer, length, flags, 0, 0);
	if(ret)
		return ret;
	TRACE(0, "[socket]: trace: recv, waiting\n");
	size_t nbytes = 0;
	while(nbytes == 0) {
		/* TODO: better blocking */
		if(nbytes == 0 && tm_process_got_signal(current_task))
			return -EINTR;
		nbytes += net_data_queue_copy_out(sock, &sock->rec_data_queue, buffer, length, (flags & MSG_PEEK), 0);
		if(!nbytes) {
			if(sock->file->flags & _FNONBLOCK)
				return nbytes;
			else
				tm_schedule();
		}
	}
	return nbytes;
}

ssize_t sys_send(int socket, const void *buffer, size_t length, int flags)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	if(!(sock->flags & SOCK_FLAG_ALLOWSEND))
		return -EIO;
	int ret = -EOPNOTSUPP;
	if(sock->calls->sendto)
		ret = sock->calls->sendto(sock, buffer, length, flags, 0, 0);
	return ret;
}

int sys_getsockname(int socket, struct sockaddr *restrict address,
		socklen_t *restrict address_len)
{
	int err;
	struct socket *sock = get_socket(socket, &err);
	if(!sock)
		return err;
	memcpy(address, &sock->local, sock->local_len);
	*address_len = sock->local_len;
	return 0;
}

int sys_recvfrom(struct socket_fromto_info *m)
{
	int err;
	struct socket *sock = get_socket(m->sock, &err);
	if(!sock)
		return err;
	if(!(sock->flags & SOCK_FLAG_ALLOWRECV))
		return -EIO;
	int ret = 0;
	if(sock->calls->recvfrom)
		ret = sock->calls->recvfrom(sock, m->buffer, m->len, m->flags, m->addr, m->addr_len);
	if(ret)
		return ret;
	TRACE(0, "[socket]: recvfrom (%x)\n", &current_task->sigd);
	size_t nbytes = 0;
	while(nbytes == 0) {
		/* TODO: better blocking */
		if(nbytes == 0 && tm_process_got_signal(current_task))
			return -EINTR;
		nbytes += net_data_queue_copy_out(sock, &sock->rec_data_queue,
				m->buffer, m->len, (m->flags & MSG_PEEK), m->addr);
		if(!nbytes) {
			if(sock->file->flags & _FNONBLOCK)
				return nbytes;
			else
				tm_schedule();
		}

	}
	if(m->addr_len)
		*m->addr_len = 16;
	return nbytes;
}

int sys_sendto(struct socket_fromto_info *m)
{
	int err;
	struct socket *sock = get_socket(m->sock, &err);
	if(!sock)
		return err;
	if(!m->addr_len)
		return -EINVAL;
	if(!(sock->flags & SOCK_FLAG_ALLOWSEND))
		return -EIO;
	int ret = -EOPNOTSUPP;
	if(sock->calls->sendto)
		ret = sock->calls->sendto(sock, m->buffer, m->len, m->flags, m->addr, *m->addr_len);
	return ret;
}

