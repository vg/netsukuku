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

#include <asm/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/in6.h>

typedef struct
{
	__u8 family;
	__u16 len;
	__u32 data[4]; 	/*The address is kept in host long format*/
} inet_prefix;

int inet_setip(inet_prefix *ip, u_int *data, u_char family)
int inet_setip_bcast(inet_prefix *ip);
int inet_setip_anyaddr(inet_prefix *ip);
char *inet_to_str(inet_prefix *ip);
int inet_to_sockaddr(inet_prefix *ip, u_short port, struct sockaddr *dst, socklen_t *dstlen);
int sockaddr_to_inet(struct sockaddr *ip, inet_prefix *dst, u_short *port);
int new_socket(int sock_type);
int new_dgram_socket(int sock_type);
int set_broadcast_sk(int socket);
int unset_broadcast_sk(int socket);
int new_broadcast_sk(int sock_type);
int new_tcp_conn(inet_prefix *host, short port);
int new_udp_conn(inet_prefix *host, short port);
int new_bcast_conn(inet_prefix *host, short port);
ssize_t inet_recv(int s, void *buf, size_t len, int flags);
ssize_t inet_recvfrom(int s, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen);
ssize_t inet_send(int s, const void *msg, size_t len, int flags);
ssize_t inet_sendto(int s, const void *msg, size_t len, int flags, const struct sockaddr *to, socklen_t tolen);
ssize_t inet_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);
