#ifndef __SEA_NET_INTERFACE_H
#define __SEA_NET_INTERFACE_H

#include <sea/types.h>
#include <sea/ll.h>
#include <sea/tm/kthread.h>
#include <sea/fs/inode.h>
#include <sea/fs/socket.h>
#include <sea/sys/ioctls.h>

#define IFF_UP      0x1     /* interface is up */
#define IFF_BROADCAST   0x2     /* broadcast address valid */
#define IFF_DEBUG   0x4     /* turn on debugging */
#define IFF_LOOPBACK    0x8     /* is a loopback net */
#define IFF_POINTOPOINT 0x10        /* interface is point-to-point link */
#define IFF_NOTRAILERS  0x20        /* avoid use of trailers */
#define IFF_RUNNING 0x40        /* resources allocated */
#define IFF_NOARP   0x80        /* no address resolution protocol */
#define IFF_PROMISC 0x100       /* receive all packets */
#define IFF_ALLMULTI    0x200       /* receive all multicast packets */
#define IFF_OACTIVE 0x400       /* transmission in progress */
#define IFF_SIMPLEX 0x800       /* can't hear own transmissions */
#define IFF_LINK0   0x1000      /* per link layer defined bit */
#define IFF_LINK1   0x2000      /* per link layer defined bit */
#define IFF_LINK2   0x4000      /* per link layer defined bit */
#define IFF_ALTPHYS IFF_LINK2   /* use alternate physical connection */
#define IFF_MULTICAST   0x8000      /* supports multicast */

#define IFACE_FLAG_UP           IFF_UP
#define IFACE_FLAG_BROADCAST    IFF_BROADCAST
#define IFACE_FLAG_DEBUG        IFF_DEBUG
#define IFACE_FLAG_LOOPBACK     IFF_LOOPBACK
#define IFACE_FLAG_PTP          IFF_POINTOPOINT
#define IFACE_FLAG_NOTRAILERS   IFF_NOTRAILERS
#define IFACE_FLAG_RUNNING      IFF_RUNNING
#define IFACE_FLAG_NOARP        IFF_NOARP
#define IFACE_FLAG_PROMISC      IFF_PROMISC
#define IFACE_FLAG_ALLMULTI     IFF_ALLMULTI
#define IFACE_FLAG_OACTIVE      IFF_OACTIVE
#define IFACE_FLAG_SIMPLEX      IFF_SIMPLEX
#define IFACE_FLAG_MULTICAST    IFF_MULTICAST

#define IFACE_FLAGS_READONLY \
	(IFF_BROADCAST | IFF_LOOPBACK | IFF_RUNNING | IFF_OACTIVE | 0xFF000000 /* bits 24-31 are reserved by kernel */)

/* these are reserved by the kernel */
#define IFACE_FLAG_ACCBROADCAST 0x10000
#define IFACE_FLAG_FORWARD      0x20000

#define IFACE_FLAGS_DEFAULT \
	(IFF_RUNNING | IFACE_FLAG_ACCBROADCAST | IFACE_FLAG_FORWARD)

#define NET_HWTYPE_LOOP     0x18
#define NET_HWTYPE_ETHERNET 0x6
#define NET_HWTYPE_MAX      0x20

#define IFNAMSIZ 16

struct net_dev {
	char name[IFNAMSIZ];
	struct inode *devnode;
	int num;
	int flags;
	uint32_t state;
	size_t rx_count, tx_count, rx_err_count, tx_err_count, rx_pending, rx_bytes, tx_bytes, collisions, brate;
	int dropped;
	time_t rx_thread_lastwork;
	/* these fields are specified by the driver at time of net_dev creation */
	struct net_dev_calls *callbacks;
	void *data; /* driver specific data */
	uint8_t hw_address[6];
	int hw_address_len;
	

	struct sockaddr addresses[AF_MAX];
	struct sockaddr masks[AF_MAX];
	struct sockaddr broadcasts[AF_MAX];
	size_t netaddr_lengths[AF_MAX];

	/*
	struct sockaddr inet_address, broad_address;
	int net_address_len;
	uint32_t netmask;
	*/
	
	
	uint16_t hw_type;
	int data_header_len;
	int mtu;

	struct linkedentry node;
	struct kthread rec_thread;
};

struct net_packet;

struct net_dev_calls {
	/* poll shall return received packets from the device in the array packets, up to
	 * the number specified by max. This call does not block, and will return no packets
	 * if none are available, and can return less than max packets if only some are
	 * available. 
	 */
	int (*poll)(struct net_dev *, struct net_packet *packets, int max);
	int (*send)(struct net_dev *, struct net_packet *packets, int count);
	int (*get_mac)(struct net_dev *, uint8_t mac[6]);
	int (*set_flags)(struct net_dev *, int);
	int (*change_link)(struct net_dev *, uint32_t);
};

int net_callback_poll(struct net_dev *, struct net_packet *, int);
int net_callback_change_link(struct net_dev *, uint32_t);
int net_callback_set_flags(struct net_dev *, int);
int net_callback_send(struct net_dev *nd, struct net_packet *packets, int count);
int net_callback_get_mac(struct net_dev *nd, uint8_t mac[6]);

struct net_dev *net_add_device(struct net_dev_calls *fn, void *);
int net_transmit_packet(struct net_dev *nd, struct net_packet *packets, int count);

int net_iface_set_flags(struct net_dev *nd, int flags);
int net_iface_get_flags(struct net_dev *nd);

void net_iface_set_netmask(struct net_dev *nd, sa_family_t af, struct sockaddr *mask);
void net_iface_get_netmask(struct net_dev *nd, sa_family_t af, struct sockaddr *mask);
void net_iface_set_bcast_addr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr);
void net_iface_get_bcast_addr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr);
void net_iface_set_netaddr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr);
void net_iface_get_netaddr(struct net_dev *nd, sa_family_t af, struct sockaddr *addr);

struct  ifreq {
	char    ifr_name[IFNAMSIZ];             /* if name, e.g. "en0" */
	union {
		struct  sockaddr ifru_addr;
		struct  sockaddr ifru_dstaddr;
		struct  sockaddr ifru_broadaddr;
		struct  sockaddr ifru_netmask;
		short   ifru_flags[2];
		short   ifru_index;
		int     ifru_metric;
		int     ifru_mtu;
		int     ifru_phys;
		int     ifru_media;
		void *ifru_data;
		int     ifru_cap[2];
	} ifr_ifru;
#define ifr_addr        ifr_ifru.ifru_addr      /* address */
#define ifr_dstaddr     ifr_ifru.ifru_dstaddr   /* other end of p-to-p link */
#define ifr_broadaddr   ifr_ifru.ifru_broadaddr /* broadcast address */
#define ifr_netmask     ifr_ifru.ifru_netmask   /* interface net mask   */
#define ifr_flags       ifr_ifru.ifru_flags[0]  /* flags */
#define ifr_prevflags   ifr_ifru.ifru_flags[1]  /* flags */
#define ifr_metric      ifr_ifru.ifru_metric    /* metric */
#define ifr_mtu         ifr_ifru.ifru_mtu       /* mtu */
#define ifr_phys        ifr_ifru.ifru_phys      /* physical wire */
#define ifr_media       ifr_ifru.ifru_media     /* physical media */
#define ifr_data        ifr_ifru.ifru_data      /* for use by interface */
#define ifr_reqcap      ifr_ifru.ifru_cap[0]    /* requested capabilities */
#define ifr_curcap      ifr_ifru.ifru_cap[1]    /* current capabilities */
#define ifr_index       ifr_ifru.ifru_index     /* interface index */
};

struct if_data {
	/* generic interface information */
	unsigned char  ifi_type;       /* ethernet, tokenring, etc */
	unsigned char  ifi_physical;       /* e.g., AUI, Thinnet, 10base-T, etc */
	unsigned char  ifi_addrlen;        /* media address length */
	unsigned char  ifi_hdrlen;     /* media header length */
	unsigned char  ifi_recvquota;      /* polling quota for receive intrs */
	unsigned char  ifi_xmitquota;      /* polling quota for xmit intrs */
	unsigned long  ifi_mtu;        /* maximum transmission unit */
	unsigned long  ifi_metric;     /* routing metric (external only) */
	unsigned long  ifi_baudrate;       /* linespeed */
	/* volatile statistics */
	unsigned long  ifi_ipackets;       /* packets received on interface */
	unsigned long  ifi_ierrors;        /* input errors on interface */
	unsigned long  ifi_opackets;       /* packets sent on interface */
	unsigned long  ifi_oerrors;        /* output errors on interface */
	unsigned long  ifi_collisions;     /* collisions on csma interfaces */
	unsigned long  ifi_ibytes;     /* total number of octets received */
	unsigned long  ifi_obytes;     /* total number of octets sent */
	unsigned long  ifi_imcasts;        /* packets received via multicast */
	unsigned long  ifi_omcasts;        /* packets sent via multicast */
	unsigned long  ifi_iqdrops;        /* dropped on input, this interface */
	unsigned long  ifi_noproto;        /* destined for unsupported protocol */
	unsigned long  ifi_hwassist;       /* HW offload capabilities */
	unsigned long  ifi_unused;     /* XXX was ifi_xmittiming */
	struct  timeval ifi_lastchange; /* time of last administrative change */
};
#define IFT_ETHER   0x6
#endif

