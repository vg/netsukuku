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

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include "inet.h"
#include "log.h"
#include "xmalloc.h"


int inet_setip(inet_prefix *ip, u_int *data, u_char family)
{
	ip->family=family;

	if(family==AF_INET) {
		memset(ip->data, '\0', sizeof(ip->data));
		ip->data[0]=ntohl(data[0]);
		ip->len=4;
	} else if(family==AF_INET6) {
		ntohl_128(data, ip->data, 1);
		ip->len=16;
	}
	else 
		return -1;
	return 0;
}

int inet_setip_bcast(inet_prefix *ip)
{
	if(ip->family==AF_INET) {
		u_int data=INADDR_BROADCAST;
		inet_setip(ip, &data, ip->family);
	} else if(ip->family==AF_INET6) {
		/*"IPv6  supports  several  address  types:  unicast  to address a single host, multicast to address a group of
		 *hosts, anycast to address the nearest member of a group of hosts (not implemented in Linux), IPv4-on-IPv6 to
		 *address a IPv4 host, and other reserved address types."
		 *DAMN!!!
		 */
		/*WE CANNOT USE ipv6! HOLY SHIT!*/
		return -1;			
	}
	else 
		return -1;

	return 0;
}

int inet_setip_anyaddr(inet_prefix *ip)
{
	if(ip->family==AF_INET) {
		u_int data=INADDR_ANY;
		inet_setip(ip, &data, ip->family);
	} else if(ip->family==AF_INET6) {
		struct in6_addr ip6=IN6ADDR_ANY_INIT;
		inet_setip(ip, &ipv6, ip->family);
	}
	else 
		return -1;

	return 0;
}
/*Coversion functions...*/

/*inet_to_str: It returns the string which represents the given ip*/
char *inet_to_str(inet_prefix *ip)
{
	char *dst;

	if(ip->family==AF_INET) {
		struct in_addr src;
		src.s_addr=htonl(ip->data[0]);
		dst=xmalloc(INET_ADDRSTRLEN);
		inet_ntop(ip->family, &src, dst, INET_ADDRSTRLEN);
	}
	else if(ip->family==AF_INET6) {
		struct in6_addr src;

		htonl_128(ip->data, &src, 1);
		dst=xmalloc(INET6_ADDRSTRLEN);
		inet_ntop(ip->family, &src, dst, INET6_ADDRSTRLEN);
	}

	return dst;
}

/*inet_to_sockaddr: Converts a inet_prefix struct to a sockaddr struct*/
int inet_to_sockaddr(inet_prefix *ip, u_short port, struct sockaddr *dst, socklen_t *dstlen)
{
	memset(dst, '\0',  sizeof(struct sockaddr));
	
	dst->sa_family=ip->family;
	port=htons(port);
	memcpy(dst->sa_data, &port, sizeof(u_short));
	
	if(ip->family==AF_INET)
		*(dst->sa_data+sizeof(u_short))=htonl(ip->data[0]);
	else if(ip->family==AF_INET6)
		htonl_128(ip->data, dst->sa_data+sizeof(u_short)+sizeof(u_int), 1);
	else
		return -1;

	if(!dstlen)
		return 0;

	*dstlen=ip->len;

	return 0;
}

int sockaddr_to_inet(struct sockaddr *ip, inet_prefix *dst, u_short *port)
{
	u_short po;
	
	memset(dst, '\0',  sizeof(inet_prefix));
	
	dst->family=ip->sa_family;
	memcpy(&po, &ip->sa_data, sizeof(u_short));
	if(port)
		*port=ntohs(po);
	
	if(ip->sa_family==AF_INET) 
		inet_setip(dst, &ip->sa_data+sizeof(u_short), ip->sa_family);
	else if(ip->sa_family==AF_INET6)
		inet_setip(dst, &ip->sa_data+sizeof(u_short)+sizeof(int), ip->sa_family);
	else
		return -1;

	return 0;
}


int new_socket(int sock_type)
{
	int sockfd;
	if((sockfd=socket(sock_type, SOCK_STREAM, 0)) == -1 ) {
		error("Socket creation failed");
		return -1;
	}

	return sockfd;
}

int new_dgram_socket(int sock_type)
{
	int sockfd;
	if((sockfd=socket(sock_type, SOCK_DGRAM, 0)) == -1 ) {
		error("Socket creation failed");
		return -1;
	}

	return sockfd;
}

int set_broadcast_sk(int socket)
{
	int broadcast=1;
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		error ("Can't set broadcasting");
		return -1;
	}
	return socket;
}

int unset_broadcast_sk(int socket)
{
	int broadcast=0;
	if (setsockopt(socket, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
		error ("Can't set broadcasting");
		return -1;
	}
}

int new_broadcast_sk(int sock_type)
{
	int sock;

	sock=new_dgram_socket(sock_type);
	return set_broadcast_sk(sock);
}

	
int new_tcp_conn(inet_prefix *host, short port)
{
	int sk, sa_len;
	struct sockaddr	sa;
	PACKET pkt;

	if(inet_to_sockaddr(host, port, &sa, &sk_len)) {
		error("Cannot new_connect(): %d Family not supported", host->family);
		return -1;
	}
	
	if((sk = new_socket(host->family)) == -1)
		return -1;

	if (connect(sk, &sa, sa_len) == -1) {
		error("Couldn't connect to %s: %s", strerror(errno));
		return -1;
	}

	/*Now we receive the first pkt from the srv. It is an ack. 
	 * Let's hope it isn't NEGATIVE (-_+)
	 */
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addsk(&pkt, sk);
	pkt_addflags(&pkt, NULL);
	pkt_recv(&pkt);

	/*Last famous words*/
	if(pkt.hdr.op==ACK_NEGATIVE) {
		int err;
		char *ntop;

		memcpy(&err, pkt.buf, pkt.hdr.sz);
		ntop=inet_to_str(host);
		error("Cannot connect to %s: %s", ntop, rq_strerror(err));

		xfree(ntop);
		pkt_free(&pkt);
		return -1;
	}

	pkt_free(&pkt);
	return sk;
}

int new_udp_conn(inet_prefix *host, short port)
{	
	int sk, sa_len;
	struct sockaddr	sa;
	PACKET pkt;

	if(inet_to_sockaddr(host, port, &sa, &sk_len)) {
		error("Cannot new_connect(): %d Family not supported", host->family);
		return -1;
	}

	if((sk = new_dgram_socket(host->family)) == -1)
		return -1;

	if (connect(sk, &sa, sa_len) == -1) {
		error("Couldn't connect to %s: %s", strerror(errno));
		return -1;
	}

	return sk;
}
	

ssize_t inet_recv(int s, void *buf, size_t len, int flags)
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=recv(s. buf, len, flags))==-1) {
		switch(errno) 
		{
			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(1, &fdset, NULL, NULL, NULL);

				if (retval == -1) {
					error("inet_recv: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_send(s, msg, len, flags);
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

	if((err=recv(s. buf, len, flags, from, fromlen))==-1) {
		switch(errno) 
		{
			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(1, &fdset, NULL, NULL, NULL);

				if (retval == -1) {
					error("inet_recv: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_send(s, msg, len, flags);
				break;

			default:
				error("inet_recv: Cannot recv(): %s", strerror(errno));
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

				ret = select(1, NULL, &fdset, NULL, NULL);

				if (retval == -1) {
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

ssize_t inet_sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
{
	ssize_t err;
	fd_set fdset;
	int ret;

	if((err=sendto(s, msg, len, flags, to, tolen))==-1) {
		switch(errno)
		{
			case EMSGSIZE:
				inet_sendto(s, msg, len/2, flags, to, tolen);
				inet_sendto(s, msg+(len/2), len-(len/2), flags, to, tolen);
				break;

			case EAGAIN:
				FD_ZERO(&fdset);
				FD_SET(s, &fdset);

				ret = select(1, NULL, &fdset, NULL, NULL);

				if (retval == -1) {
					error("inet_sendto: select error: %s", strerror(errno));
					return err;
				}

				if(FD_ISSET(s, &fdset))
					inet_sendto(s, msg, len, flags, to, tolen);
				break;

			default:
				error("inet_sendto: Cannot send(): %s", strerror(errno));
				return err;
				/*not reached*/
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
			FD_SET(s, &fdset);

			ret = select(1, NULL, &fdset, NULL, NULL);

			if (retval == -1) {
				error("inet_sendfile: select error: %s", strerror(errno));
				return err;
			}

			if(FD_ISSET(s, &fdset))
				inet_sendfile(s, msg, len, flags, to, tolen);
		}
		else {
			error("inet_sendfile: Cannot sendfile(): %s", strerror(errno));
			return err;

		}
	}
		return err;
}
