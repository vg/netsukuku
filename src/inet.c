/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

/* * * Coversion functions... * * */

/*
 * inet_to_str: It returns the string which represents the given ip.
 * It must be xfreed after.
 */
char *inet_to_str(inet_prefix ip)
{
	struct in_addr src, src6;
	char *dst;

	if(ip.family==AF_INET) {
		src.s_addr=htonl(ip.data[0]);
		dst=xmalloc(INET_ADDRSTRLEN);
		inet_ntop(ip.family, &src, dst, INET_ADDRSTRLEN);
	} else if(ip.family==AF_INET6) {
		htonl_128(ip.data, &src6);
		dst=xmalloc(INET6_ADDRSTRLEN);
		inet_ntop(ip.family, &src6, dst, INET6_ADDRSTRLEN);
	}

	return dst;
}

/*
 * inet_to_sockaddr: Converts a inet_prefix struct to a sockaddr struct
 */
int inet_to_sockaddr(inet_prefix *ip, u_short port, struct sockaddr *dst, socklen_t *dstlen)
{
	char *p;
	int data[4]={0,0,0,0};
	
	memset(dst, '\0',  sizeof(struct sockaddr));
	
	dst->sa_family=ip->family;
	port=htons(port);
	memcpy(dst->sa_data, &port, sizeof(u_short));
	
	if(ip->family==AF_INET) {
		data[0]=htonl(ip->data[0]);
		p=(char *)dst->sa_data+2;
		memcpy(p, data, sizeof(int)*4);
	} else if(ip->family==AF_INET6) {
		p=(char *)dst->sa_data+sizeof(u_short)+sizeof(u_int);
		htonl_128(ip->data, p);
	} else
		return -1;

	if(dstlen)
		*dstlen=ip->len;

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
	
	if(ip->sa_family==AF_INET) {
		p=(char *)ip->sa_data+sizeof(u_short);
		inet_setip(dst, (u_int *)p, ip->sa_family);
	} else if(ip->sa_family==AF_INET6) {
		p=(char *)ip->sa_data+sizeof(u_short)+sizeof(int);
		inet_setip(dst, (u_int *)p, ip->sa_family);
	} else
		return -1;

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
	int loop_back=0;
	struct ipv6_mreq    mreq6;
	struct in6_addr     addr;
	int addr32[4]=IPV6_ADDR_BROADCAST;
	
	memcpy(&addr, addr32, sizeof(int)*4);

	if (bind(socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		error("bind error while joining in the multicast group");
		close(socket);
		return -1;
	}
	memset(&mreq6, 0, sizeof(struct ipv6_mreq));
	memcpy(&mreq6.ipv6mr_multiaddr,	&addr, sizeof(struct in6_addr));
	mreq6.ipv6mr_interface=idx;
	
	if(setsockopt(socket, IPPROTO_IPV6, IPV6_ADD_MEMBERSHIP, &mreq6, 
				sizeof(mreq6)) < 0) {
		error("Cannot set IPV6_ADD_MEMBERSHIP: %s", strerror(errno));
	        close(socket);
		return -1;
	}

	if(setsockopt(socket, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop_back, 
				sizeof(loop_back)) < 0) {
		error("Cannot set IPV6_MULTICAST_LOOP: %s", strerror(errno));
	        close(socket);
		return -1;
	}
	
	return socket;
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
	 * TIME_WAIT state.
	 */
	ret=setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &reuseaddr, sizeof(int));
	if(ret < 0)
		error("setsockopt SO_REUSEADDR: %s", strerror(errno));
	return ret;
}

int set_multicast_loop_sk(int family, int socket)
{
	int ret;
	u_char loop=0;    /* 0 = disable, 1 = enable (default) */
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

int set_broadcast_sk(int socket, int family, int dev_idx)
{
	int broadcast=1;
	if(family == AF_INET) {
		if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
			error("Cannot set broadcasting: %s", strerror(errno));
			close(socket);
			return -1;
		}
	} else if(family == AF_INET6) {
		if(join_ipv6_multicast(socket, dev_idx) < 0) {
			return -1;
		}
	}
	
	set_multicast_loop_sk(family, socket);

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
}

int new_broadcast_sk(int family, int dev_idx)
{
	int sock;

	sock=new_dgram_socket(family);
	return set_broadcast_sk(sock, family, dev_idx);
}
	
int new_tcp_conn(inet_prefix *host, short port)
{
	int sk, sa_len;
	struct sockaddr	sa;
	char *ntop;
	ntop=inet_to_str(*host);
	
	if(inet_to_sockaddr(host, port, &sa, &sa_len)) {
		error("Cannot new_tcp_connect(): %d Family not supported", host->family);
		sk=-1;
		goto finish;
	}
	
	if((sk = new_socket(host->family)) == -1) {
		sk=-1;
		goto finish;
	}

	if (connect(sk, &sa, sa_len) == -1) {
		error("Cannot connect to %s: %s", ntop, strerror(errno));
		sk=-1;
		goto finish;
	}
finish:
	xfree(ntop);
	return sk;
}

int new_udp_conn(inet_prefix *host, short port)
{	
	int sk, sa_len;
	struct sockaddr	sa;
	char *ntop;
	ntop=inet_to_str(*host);

	if(inet_to_sockaddr(host, port, &sa, &sa_len)) {
		error("Cannot new_udp_connect(): %d Family not supported", host->family);
		sk=-1;
		goto finish;
	}

	if((sk = new_dgram_socket(host->family)) == -1) {
		sk=-1;
		goto finish;
	}

	if (connect(sk, &sa, sa_len) == -1) {
		error("Cannot connect to %s: %s", ntop, strerror(errno));
		sk=-1;
		goto finish;
	}
	
finish:
	xfree(ntop);
	return sk;
}
	
int new_bcast_conn(inet_prefix *host, short port, int dev_idx) 
{	
	int sk, sa_len;
	struct sockaddr	sa;
	PACKET pkt;
	char *ntop;

	if(inet_to_sockaddr(host, port, &sa, &sa_len)) {
		error("Cannot new_bcast_connect(): %d Family not supported", 
				host->family);
		return -1;
	}

	if((sk = new_broadcast_sk(host->family, dev_idx)) == -1)
		return -1;
	
	if(host->family == AF_INET)
		if(connect(sk, &sa, sizeof(sa)) == -1) {
			ntop=inet_to_str(*host);
			error("Cannot connect to the broadcast (%s): %s", ntop,
					strerror(errno));
			xfree(ntop);
			return -1;
		}

	return sk;
}
	
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

	if((err=recvfrom(s, buf, len, flags, from, fromlen))==-1) {
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

	if((err=send(s, msg, len, flags))==-1) {
		switch(errno) 
		{
			case EMSGSIZE:
				inet_send(s, msg, len/2, flags);
				inet_send(s, msg+(len/2), len-(len/2), flags);
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
				inet_sendto(s, (const void *)(msg+(len/2)), len-(len/2), flags, to, tolen);
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
