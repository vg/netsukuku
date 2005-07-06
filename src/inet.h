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

/*
 * This is the "link-scope all-hosts multicast" address: ff02::1.
 * Each element is in network order.
 */
#define IPV6_ADDR_BROADCAST		{ 0x2ff, 0x0, 0x0, 0x1000000 }

/* in network byte order */
#define LOOPBACK_IP			0x7f000001
#define LOOPBACK_NET			0x7f000000
#define LOOPBACK_BCAST			0x7fffffff

#define LOOPBACK_IP_V6			0x1

/* in host byte order */
#define NTK_PRIVATE_CLASS_MASK_IPV4	0x0a000000	/* 10.x.x.x */
#define NTK_PRIVATE_CLASS_MASK_IPV6	0xfec00000	/* fec0:xxxx:... */

typedef struct
{
	u_char	family;
	u_short len;
	u_char	bits;
	u_int	data[4]; 	/* The address is kept in host long format, 
				   word ORDER 1 (most significant word first)*/
}inet_prefix;

/* * * defines from linux/in.h * * */
#define LOOPBACK(x)	(((x) & htonl(0xff000000)) == htonl(0x7f000000))
#define MULTICAST(x)	(((x) & htonl(0xf0000000)) == htonl(0xe0000000))
#define BADCLASS(x)	(((x) & htonl(0xf0000000)) == htonl(0xf0000000))
#define ZERONET(x)	(((x) & htonl(0xff000000)) == htonl(0x00000000))
#define LOCAL_MCAST(x)	(((x) & htonl(0xFFFFFF00)) == htonl(0xE0000000))

/* * * defines from linux/include/net/ipv6.h * * */
#define IPV6_ADDR_ANY		0x0000U

#define IPV6_ADDR_UNICAST      	0x0001U	
#define IPV6_ADDR_MULTICAST    	0x0002U	

#define IPV6_ADDR_LOOPBACK	0x0010U
#define IPV6_ADDR_LINKLOCAL	0x0020U
#define IPV6_ADDR_SITELOCAL	0x0040U

#define IPV6_ADDR_COMPATv4	0x0080U

#define IPV6_ADDR_SCOPE_MASK	0x00f0U

#define IPV6_ADDR_MAPPED	0x1000U
#define IPV6_ADDR_RESERVED	0x2000U	/* reserved address space */

/* * * Functions declaration * * */
void inet_ntohl(inet_prefix *ip);
void inet_htonl(inet_prefix *ip);
int inet_setip(inet_prefix *ip, u_int *data, int family);
int inet_setip_bcast(inet_prefix *ip, int family);
int inet_setip_anyaddr(inet_prefix *ip, int family);
int inet_setip_anyaddr(inet_prefix *ip, int family);
int inet_setip_localaddr(inet_prefix *ip, int family);
int inet_addr_match(const inet_prefix *a, const inet_prefix *b, int bits);
int ipv6_addr_type(inet_prefix addr);
int inet_validate_ip(inet_prefix ip);

const char *inet_to_str(inet_prefix ip);
int inet_to_sockaddr(inet_prefix *ip, u_short port, struct sockaddr *dst, socklen_t *dstlen);
int sockaddr_to_inet(struct sockaddr *ip, inet_prefix *dst, u_short *port);

int new_socket(int sock_type);
int new_dgram_socket(int sock_type);
int join_ipv6_multicast(int socket, int idx);

int set_nonblock_sk(int fd);
int unset_nonblock_sk(int fd);
int set_reuseaddr_sk(int socket);
int set_bindtodevice_sk(int socket, char *dev);
int new_broadcast_sk(int family, int dev_idx);

int new_tcp_conn(inet_prefix *host, short port);
int new_udp_conn(inet_prefix *host, short port);
int new_bcast_conn(inet_prefix *host, short port, int dev_idx);

ssize_t inet_recv(int s, void *buf, size_t len, int flags);
ssize_t inet_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t inet_send(int s, const void *msg, size_t len, int flags);
ssize_t inet_sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t inet_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
