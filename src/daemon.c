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

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>

#include "netsukuku.h"

extern int my_family, ntk_udp_port, ntk_tcp_port;
extern struct current me;

/* 
 * prepare_listen_socket: 
 * It creates a new socket of the desired `family' and binds it to the 
 * specified `port'. It sets also the reuseaddr and NONBLOCK
 * socket options, because this new socket shall be used to listen() and
 * accept().
 */
int prepare_listen_socket(int family, int socktype, u_short port) 
{
	struct addrinfo hints, *ai, *aitop;
	char strport[NI_MAXSERV];
	int err, s;

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family=family;
	hints.ai_socktype=socktype;
	hints.ai_flags=AI_PASSIVE;
	snprintf(strport, NI_MAXSERV, "%u", port);
	
	err=getaddrinfo(NULL, strport, &hints, &aitop);
	if(err) {
		error("Getaddrinfo error: %s", gai_strerror(err));
		return -1;
	}

	for (ai = aitop; ai->ai_next; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1)
			/* Maybe we can use another socket...*/
			continue;

		set_reuseaddr_sk(s);

		/* Let's bind it! */
		if(bind(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			close(s);
			continue;
		}
		freeaddrinfo(aitop);
	}
	
	error("Cannot open inbound socket on port %d: %s", port, strerror(errno));
	freeaddrinfo(all_ai);
	return -1;
}

void *daemon_udp(void *null)
{
	PACKET rpkt;
	fd_set fdset;
	int sk, ret;
	char *ntop;

	sk=prepare_listen_socket(my_family, SOCK_DGRAM, ntk_udp_port);
	if(sk == -1)
		return;

	set_broadcast_sk(sk, my_family, me.cur_dev_idx);

	for(;;) {
		FD_ZERO(&fdset);
		FD_SET(sk, &fdset);

		ret = select(sk+1, &fdset, NULL, NULL, NULL);
		if (ret < 0 && errno != EINTR) {
			error("daemon_tcp: select error: %s", strerror(errno));
			continue;
		}

		if(!FD_ISSET(sk, &fdset))
			continue;

		pkt_addsk(&rpkt, sk, SKT_UDP);
		pkt_addflags(&rpkt, MSG_WAITALL);
		inet_recv(&rpkt):
			
		if(add_accept(rpkt.from, 1)) {
			ntop=inet_to_str(&rpkt.from);
			debug(DBG_NORMAL, "ACPT: dropped UDP pkt from %s: Accept table full.", ntop);
			xfree(ntop);
			continue;
		} 
		
		pkt_exec(rpkt, accept_idx);
		pkt_free(&rpkt, 0);
	}
}

void *recv_loop(void *recv_pkt)
{
	PACKET rpkt;
	ssize_t ret;
	int acpt_idx, acpt_sidx;

	acpt_idx=accept_idx;
	acpt_sidx=accept_sidx;
	add_accept_pid(getpid(), acpt_idx, acpt_sidx);
	
	memcpy(&rpkt, recv_pkt, sizeof(PACKET));

	while( inet_recv(&rpkt) != -1 ) {
		pkt_exec(rpkt, acpt_idx);
		pkt_free(&rpkt, 0);
	}

	close_accept(acpt_idx, acpt_sidx);
}

void *daemon_tcp(void *null)
{
	pthread_t thread;
	pthread_attr_t t_attr;
	
	PACKET rpkt;
	inet_prefix ip;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof addr;
	fd_set fdset;
	int sk, fd, ret;
	char *ntop;

	sk=prepare_listen_socket(my_family, SOCK_STREAM, ntk_tcp_port);
	if(sk == -1)
		return;

	/* 
	 * While we are accepting the connections we keep the socket non
	 * blocking.
	 */
	if(set_nonblock_sk(sk))
		return;

	/* Shhh, it's listening... */
	if(listen(sk, 5) == -1) {
		close(sk);
		return;
	}

	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

	for(;;) {
		memset(&rpkt, 0, sizeof(PACKET));
		FD_ZERO(&fdset);
		FD_SET(sk, &fdset);

		ret = select(sk+1, &fdset, NULL, NULL, NULL);
		if(ret < 0 && errno != EINTR)
			error("daemon_tcp: select error: %s", strerror(errno));
		if(ret < 0)
			continue;
		
		if(!FD_ISSET(sk, &fdset))
			continue;

		fd=accept(sk, (struct sockaddr *)&addr, &addrlen);
		if(fd == -1) {
			if (errno != EINTR && errno != EWOULDBLOCK)
				error("daemon_tcp: accept(): %s", strerror(errno));
			continue;
		}
		
		sockaddr_to_inet(&addr, &ip, 0);
		if(ret=add_accept(ip, 0)) {
			ntop=inet_to_str(&ip);
			debug(DBG_NORMAL, "ACPT: drop connection with %s: Accept table full.", ntop);
			xfree(ntop);
			
			/* Omg, we cannot take it anymore, go away: ACK_NEGATIVE */
			pkt_err(pkt, ret);
			close(newsock);
			continue;
		} else {
			ntop=inet_to_str(&ip);
			debug(DBG_NORMAL, "ACPT: Accept_tbl ok! accept_idx: %d from %s", accept_idx, ntop);
			xfree(ntop);
			
			/* 
			 * Ok, the connection is good, send back the
			 * ACK_AFFERMATIVE.
			 */
			pkt_addto(&pkt, &pkt.from);
			send_rq(&pkt, 0, ACK_AFFERMATIVE, 0, 0, 0, 0);
		}

		if(unset_nonblock(fd))
			continue;
	
		pkt_addsk(&rpkt, fd, SKT_TCP);
		pkt_addflags(&rpkt, MSG_WAITALL);
		pthread_create(&thread, &t_attr, recv_loop, &rpkt);
	}
	
	destroy_accept_tbl();
}
