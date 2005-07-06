/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Public License as published 
 * by the Free Software Foundation; either version 2 of the License,
 * or (at your option) any later version.
 *
 * This source code is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * Please refer to the GNU Public License for more details.
 *
 * You should have received a copy of the GNU Public License along with
 * this source code; if not, write to:
 * Free Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "includes.h"

#include "ipv6-gmp.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "inet.h"
#include "pkts.h"
#include "request.h"
#include "log.h"
#include "xmalloc.h"

extern int errno;

/* 
 * inet_ntohl: Converts the ip->data from network to host order 
 */
void inet_ntohl(inet_prefix *ip)
{
	if(ip->family==AF_INET) {
		ip->data[0]=ntohl(ip->data[0]);
	} else
		ntohl_128(ip->data, ip->data);
}

/* 
 * inet_ntohl: Converts the ip->data from host to network order 
 */
void inet_htonl(inet_prefix *ip)
{
	if(ip->family==AF_INET) {
		ip->data[0]=htonl(ip->data[0]);
	} else
		htonl_128(ip->data, ip->data);
}

int inet_setip(inet_prefix *ip, u_int *data, int family)
{
	ip->family=family;
	memset(ip->data, '\0', sizeof(ip->data));
	
	if(family==AF_INET) {
		ip->data[0]=data[0];
		ip->len=4;
	} else if(family==AF_INET6) {
		memcpy(ip->data, data, sizeof(ip->data));
		ip->len=16;
	} else 
		return -1;

	inet_ntohl(ip);
	ip->bits=ip->len*8;
	return 0;
}

int inet_setip_bcast(inet_prefix *ip, int family)
{
	if(family==AF_INET) {
		u_int data[4]={0, 0, 0, 0};
		data[0]=INADDR_BROADCAST;
		inet_setip(ip, data, family);
	} else if(family==AF_INET6) {
		u_int data[4]=IPV6_ADDR_BROADCAST;
		inet_setip(ip, data, family);
	} else 
		return -1;

	return 0;
}

int inet_setip_anyaddr(inet_prefix *ip, int family)
{
	if(family==AF_INET) {
		u_int data[4]={0, 0, 0, 0};
		
		data[0]=INADDR_ANY;
		inet_setip(ip, data, family);
	} else if(family==AF_INET6) {
		struct in6_addr ipv6=IN6ADDR_ANY_INIT;
		inet_setip(ip, (u_int *)(&ipv6), family);
	} else 
		return -1;

	return 0;
}

/* 
 * inet_setip_localaddr: Restrict the `ip' to a local private class changing the
 * first byte of the `ip'. In the ipv4 the CLASS A is used, in ipv6 the site
 * local class.
 */
int inet_setip_localaddr(inet_prefix *ip, int family)
{
	if(family==AF_INET) {
		ip->data[0] = (ip->data[0] & ~0xff000000)|NTK_PRIVATE_CLASS_MASK_IPV4;
	} else if(family==AF_INET6) {
		ip->data[0] = (ip->data[0] & ~0xffff0000)|NTK_PRIVATE_CLASS_MASK_IPV6;
	} else 
		return -1;

	return 0;
}

/* from iproute2 */
int inet_addr_match(const inet_prefix *a, const inet_prefix *b, int bits)
{
        __u32 *a1 = a->data;
        __u32 *a2 = b->data;
        int words = bits >> 0x05;
        
        bits &= 0x1f;
        
        if (words)
                if (memcmp(a1, a2, words << 2))
                        return -1;

        if (bits) {
                __u32 w1, w2;
                __u32 mask;

                w1 = a1[words];
                w2 = a2[words];

                mask = htonl((0xffffffff) << (0x20 - bits));

                if ((w1 ^ w2) & mask)
                        return 1;
        }

	return 0;
}

/* from linux/net/ipv6/addrconf.c. Modified to use inet_prefix */
int ipv6_addr_type(inet_prefix addr)
{
	int type;
	u_int st;

	st = htonl(addr.data[0]);

	if ((st & htonl(0xFF000000)) == htonl(0xFF000000)) {
		type = IPV6_ADDR_MULTICAST;

		switch((st & htonl(0x00FF0000))) {
			case __constant_htonl(0x00010000):
				type |= IPV6_ADDR_LOOPBACK;
				break;

			case  __constant_htonl(0x00020000):
				type |= IPV6_ADDR_LINKLOCAL;
				break;

			case  __constant_htonl(0x00050000):
				type |= IPV6_ADDR_SITELOCAL;
				break;
		};
		return type;
	}

	type = IPV6_ADDR_UNICAST;

	/* Consider all addresses with the first three bits different of
	   000 and 111 as finished.
	 */
	if ((st & htonl(0xE0000000)) != htonl(0x00000000) &&
	    (st & htonl(0xE0000000)) != htonl(0xE0000000))
		return type;
	
	if ((st & htonl(0xFFC00000)) == htonl(0xFE800000))
		return (IPV6_ADDR_LINKLOCAL | type);

	if ((st & htonl(0xFFC00000)) == htonl(0xFEC00000))
		return (IPV6_ADDR_SITELOCAL | type);

	if ((addr.data[0] | addr.data[1]) == 0) {
		if (addr.data[2] == 0) {
			if (addr.data[3] == 0)
				return IPV6_ADDR_ANY;

			if (htonl(addr.data[3]) == htonl(0x00000001))
				return (IPV6_ADDR_LOOPBACK | type);

			return (IPV6_ADDR_COMPATv4 | type);
		}

		if (htonl(addr.data[2]) == htonl(0x0000ffff))
			return IPV6_ADDR_MAPPED;
	}

	st &= htonl(0xFF000000);
	if (st == 0)
		return IPV6_ADDR_RESERVED;
	st &= htonl(0xFE000000);
	if (st == htonl(0x02000000))
		return IPV6_ADDR_RESERVED;	/* for NSAP */
	if (st == htonl(0x04000000))
		return IPV6_ADDR_RESERVED;	/* for IPX */
	return type;
}

int inet_validate_ip(inet_prefix ip)
{
	int type, ipv4;

	if(ip.family==AF_INET) {
		ipv4=htonl(ip.data[0]);
		if(MULTICAST(ipv4) || BADCLASS(ipv4) || ZERONET(ipv4))
			return -EINVAL;
		return 0;

	} else if(ip.family==AF_INET6) {
		type=ipv6_addr_type(ip);
		if( (type & IPV6_ADDR_MULTICAST) || (type & IPV6_ADDR_RESERVED) || 
				(type & IPV6_ADDR_LOOPBACK))
			return -EINVAL;
		return 0;
	}

	return -EINVAL;
}

/* * * Coversion functions... * * */

/*
 * inet_to_str: It returns the string which represents the given ip.
 */
const char *inet_to_str(inet_prefix ip)
{
	struct in_addr src;
	struct in6_addr src6;
	static char dst[INET_ADDRSTRLEN], dst6[INET6_ADDRSTRLEN];

	if(ip.family==AF_INET) {
		src.s_addr=htonl(ip.data[0]);
		inet_ntop(ip.family, &src, dst, INET_ADDRSTRLEN);
		
		return dst;
	} else if(ip.family==AF_INET6) {
		htonl_128(ip.data, (u_int *)&src6);
		inet_ntop(ip.family, &src6, dst6, INET6_ADDRSTRLEN);

		return dst6;
	}

	return 0;
}

/*
 * inet_to_sockaddr: Converts a inet_prefix struct to a sockaddr struct
 */
int inet_to_sockaddr(inet_prefix *ip, u_short port, struct sockaddr *dst, socklen_t *dstlen)
{
	port=htons(port);
	
	if(ip->family==AF_INET) {
		struct sockaddr_in sin;
		memset(&sin, '\0',  sizeof(struct sockaddr_in));
		
		sin.sin_family = ip->family;
		sin.sin_port = port;
		sin.sin_addr.s_addr = htonl(ip->data[0]);
		memcpy(dst, &sin, sizeof(struct sockaddr_in));
		
		if(dstlen)
			*dstlen=sizeof(struct sockaddr_in);

	} else if(ip->family==AF_INET6) {
		struct sockaddr_in6 sin6;
		memset(&sin6, '\0',  sizeof(struct sockaddr_in6));
		
		sin6.sin6_family = ip->family;
		sin6.sin6_port = port;
		sin6.sin6_flowinfo = 0;
		htonl_128(ip->data, sin6.sin6_addr.s6_addr32);

		memcpy(dst, &sin6, sizeof(struct sockaddr_in6));

		if(dstlen)
			*dstlen=sizeof(struct sockaddr_in6);
	} else
		return -1;

	return 0;
}

int sockaddr_to_inet(struct sockaddr *ip, inet_prefix *dst, u_short *port)
{
	u_short po;
	char *p;
	
	memset(dst, '\0',  sizeof(inet_prefix));
	
	dst->family=ip->sa_family;
	memcpy(&po, &ip->sa_data, sizeof(u_short));
	if(port)
		*port=ntohs(po);
	
	if(ip->sa_family==AF_INET)
		p=(char *)ip->sa_data+sizeof(u_short);
	else if(ip->sa_family==AF_INET6)
		p=(char *)ip->sa_data+sizeof(u_short)+sizeof(int);
	else
		return -1;
		
	inet_setip(dst, (u_int *)p, ip->sa_family);

	return 0;
}


int new_socket(int sock_type)
{
	int sockfd;
	if((sockfd=socket(sock_type, SOCK_STREAM, 0)) == -1 ) {
		error("Socket SOCK_STREAM creation failed: %s", strerror(errno));
		return -1;
	}

	return sockfd;
}

int new_dgram_socket(int sock_type)
{
	int sockfd;
	if((sockfd=socket(sock_type, SOCK_DGRAM, 0)) == -1 ) {
		error("Socket SOCK_DGRAM creation failed: %s", strerror(errno));
		return -1;
	}

	return sockfd;
}


/* 
 * join_ipv6_multicast: It adds the membership to the IPV6_ADDR_BROADCAST
 * multicast group. The device with index `idx' will be used. 
 */
int join_ipv6_multicast(int socket, int idx)
{
	struct ipv6_mreq    mreq6;
	const int addr[4]=IPV6_ADDR_BROADCAST;
	
	memset(&mreq6, 0, sizeof(struct ipv6_mreq));
	memcpy(&mreq6.ipv6mr_multiaddr,	addr, sizeof(struct in6_addr));
	mreq6.ipv6mr_interface=idx;
	
	if(setsockopt(socket, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq6, 
				sizeof(mreq6)) < 0) {
		error("Cannot set IPV6_JOIN_GROUP: %s", strerror(errno));
	        close(socket);
		return -1;
	}

	return socket;
}

int set_multicast_if(int socket, int idx)
{
	/* man ipv6 */

	if (setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_IF,
				&idx, sizeof(int)) < 0) {
		error("set_multicast_if(): cannot set IPV6_MULTICAST_IF: %s",
				strerror(errno));
		close(socket);
		return -1;
	}

	return 0;
}
		
int set_nonblock_sk(int fd)
{
	if (fcntl(fd, F_SETFL, O_NONBLOCK) < 0) {
		error("set_nonblock_sk(): cannot set O_NONBLOCK: %s", 
				strerror(errno));
		close(fd);
		return -1;
	}
	return 0;
}

int unset_nonblock_sk(int fd)
{
	if (fcntl(fd, F_SETFL, 0) < 0) {
		error("unset_nonblock_sk(): cannot unset O_NONBLOCK: %s", 
				strerror(errno));
		close(fd);
		return -1;
	}
	return 0;
}

int set_reuseaddr_sk(int socket)
{
	int reuseaddr=1, ret;
	/*
	 * SO_REUSEADDR: <<Go ahead and reuse that port even if it is in
	 * TIME_WAIT state.>>
	 */
	ret=setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
	if(ret < 0)
		error("setsockopt SO_REUSEADDR: %s", strerror(errno));
	return ret;
}

int set_bindtodevice_sk(int socket, char *dev)
{
	struct ifreq ifr;
	int ret;
	
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, dev, IFNAMSIZ-1);
	
	ret=setsockopt(socket, SOL_SOCKET, SO_BINDTODEVICE, dev, strlen(dev)+1);
	if(ret < 0)
		error("setsockopt SO_BINDTODEVICE: %s", strerror(errno));

        return ret;
}

/*
 * `loop': 0 = disable, 1 = enable (default) 
 */
int set_multicast_loop_sk(int family, int socket, u_char loop)
{
	int ret=0;

	/*
	 * <<The IPV6_MULTICAST_LOOP option gives the sender explicit control
	 * over whether or not subsequent datagrams are looped bac.>>
	 */
	if(family==AF_INET6)
		ret=setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop, sizeof(loop));
	if(ret < 0)
		error("setsockopt IP_MULTICAST_LOOP: %s", strerror(errno));
	return ret;
}

int set_broadcast_sk(int socket, int family, inet_prefix *host, short port,
		int dev_idx)
{
	struct sockaddr_storage saddr_sto;
	struct sockaddr	*sa=(struct sockaddr *)&saddr_sto;
	socklen_t alen;
	int broadcast=1;
	char *device;
	
	if(family == AF_INET) {
		if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &broadcast,
					sizeof(broadcast)) < 0) {
			error("Cannot set SO_BROADCAST to socket: %s", strerror(errno));
			close(socket);
			return -1;
		}
	} else if(family == AF_INET6) {
		if(join_ipv6_multicast(socket, dev_idx) < 0)
			return -1;
		if(set_multicast_loop_sk(family, socket, 0) < 0)
			return -1;
		set_multicast_if(socket, dev_idx);
	}
	
	device=(char *)ll_index_to_name(dev_idx);
	set_bindtodevice_sk(socket, device);

	/* Let's bind it! */
	alen = sizeof(saddr_sto);
	memset(sa, 0, alen);
	if (getsockname(socket, sa, &alen) == -1) {
		error("Cannot getsockname: %s", strerror(errno));
		close(socket);
		return -1;
	}
	
	if(bind(socket, sa, alen) < 0) {
		error("Cannot bind the broadcast socket: %s", strerror(errno));
		close(socket);
		return -1;
	}
	
	return socket;
}

int unset_broadcast_sk(int socket, int family)
{
	int broadcast=0;
	if(family == AF_INET) {
		if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			error ("Cannot unset broadcasting: %s", strerror(errno));
			return -1;
		}
	}
	return 0;
}


/* 
 *  *  *  Connection functions  *  *  *
 */

int new_tcp_conn(inet_prefix *host, short port)
{
	int sk;
	socklen_t sa_len;
	struct sockaddr_storage saddr_sto;
	struct sockaddr	*sa=(struct sockaddr *)&saddr_sto;
	const char *ntop;
	ntop=inet_to_str(*host);
	
	if(inet_to_sockaddr(host, port, sa, &sa_len)) {
		error("Cannot new_tcp_connect(): %d Family not supported", host->family);
		sk=-1;
		goto finish;
	}
	
	if((sk = new_socket(host->family)) == -1) {
		sk=-1;
		goto finish;
	}

	if (connect(sk, sa, sa_len) == -1) {
		error("Cannot tcp_connect() to %s: %s", ntop, strerror(errno));
		sk=-1;
		goto finish;
	}
finish:
	return sk;
}

int new_udp_conn(inet_prefix *host, short port)
{	
	int sk;
	socklen_t sa_len;
	struct sockaddr_storage saddr_sto;
	struct sockaddr	*sa=(struct sockaddr *)&saddr_sto;
	const char *ntop;
	ntop=inet_to_str(*host);

	if(inet_to_sockaddr(host, port, sa, &sa_len)) {
		error("Cannot new_udp_connect(): %d Family not supported", host->family);
		sk=-1;
		goto finish;
	}

	if((sk = new_dgram_socket(host->family)) == -1) {
		sk=-1;
		goto finish;
	}

	if (connect(sk, sa, sa_len) == -1) {
		error("Cannot connect to %s: %s", ntop, strerror(errno));
		sk=-1;
		goto finish;
	}
	
finish:
	return sk;
}
	
int new_bcast_conn(inet_prefix *host, short port, int dev_idx)
{	
	struct sockaddr_storage saddr_sto;
	struct sockaddr	*sa=(struct sockaddr *)&saddr_sto;
	socklen_t alen;
	int sk;
	const char *ntop;

	if((sk = new_dgram_socket(host->family)) == -1)
		return -1;
	sk=set_broadcast_sk(sk, host->family, host, port, dev_idx);
	
	/*
	 * Connect 
	 */
	if(inet_to_sockaddr(host, port, sa, &alen)) {
		error("set_broadcast_sk: %d Family not supported", host->family);
		return -1;
	}
	
	if(host->family == AF_INET6) {
		struct sockaddr_in6 *sin6=(struct sockaddr_in6 *)sa;
		sin6->sin6_scope_id = dev_idx;
	}
	
	if(connect(sk, sa, alen) == -1) {
		ntop=inet_to_str(*host);
		error("Cannot connect to the broadcast (%s): %s", ntop,	
				strerror(errno));
		return -1;
	}

	return sk;
}


/* 
 *  *  *  Recv/Send functions  *  *  *
 */

ssize_t inet_recv(int s, void *buf, size_t len, int flags)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=recv(s, buf, len, flags))==-1) {
		switch(errno) 
		{
			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(s+1, &fdset, NULL, NULL, NULL);
				if (ret == -1) {
					error("inet_recv: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_recv(s, buf, len, flags);
				break;

			default:
				error("inet_recv: Cannot recv(): %s", strerror(errno));
				return err;
				break;
		}
	}
	return err;
}

ssize_t inet_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=recvfrom(s, buf, len, flags, from, fromlen)) < 0) {
		switch(errno) 
		{
			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(s+1, &fdset, NULL, NULL, NULL);

				if (ret == -1) {
					error("inet_recvfrom: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_recv(s, buf, len, flags);
				break;

			default:
				error("inet_recvfrom: Cannot recv(): %s", strerror(errno));
				return err;
				break;
		}
	}
	return err;
}

			
ssize_t inet_send(int s, const void *msg, size_t len, int flags)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=send(s, msg, len, flags)) < 0) {
		switch(errno) 
		{
			case EMSGSIZE:
				inet_send(s, msg, len/2, flags);
				inet_send(s, (const char *)msg+(len/2), 
						len-(len/2), flags);
				break;

			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(s+1, NULL, &fdset, NULL, NULL);

				if (ret == -1) {
					error("inet_send: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_send(s, msg, len, flags);
				break;

			default:
				error("inet_send: Cannot send(): %s", strerror(errno));
				return err;
				break;
		}
	}
	return err;
}

ssize_t inet_sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=sendto(s, msg, len, flags, to, tolen))==-1) {
		switch(errno)
		{
			case EMSGSIZE:
				inet_sendto(s, msg, len/2, flags, to, tolen);
				inet_sendto(s, ((const char *)msg+(len/2)), 
						len-(len/2), flags, to, tolen);
				break;

			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(s+1, NULL, &fdset, NULL, NULL);
				if (ret == -1) {
					error("inet_sendto: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_sendto(s, msg, len, flags, to, tolen);
				break;

			default:
				error("inet_sendto: Cannot send(): %s", strerror(errno));
				return err;
				break;
		}
	}
	return err;
}

ssize_t inet_sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=sendfile(out_fd, in_fd, offset, count))==-1) {
		if(errno==EAGAIN) {
			FD_ZERO(&fdset);
			FD_SET(out_fd, &fdset);

			ret = select(out_fd+1, NULL, &fdset, NULL, NULL);

			if (ret == -1) {
				error("inet_sendfile: select error: %s", strerror(errno));
				return err;
			}

			if(FD_ISSET(out_fd, &fdset))
				inet_sendfile(out_fd, in_fd, offset, count);
		}
		else {
			error("inet_sendfile: Cannot sendfile(): %s", strerror(errno));
			return err;

		}
	}
		return err;
}
