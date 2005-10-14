/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
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
#include "request.h"
#include "endianness.h"
#include "pkts.h"
#include "daemon.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "netsukuku.h"
#include "accept.h"
#include "xmalloc.h"
#include "log.h"

extern int errno;

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

	for (ai = aitop; ai; ai = ai->ai_next) {
		if (ai->ai_family != AF_INET && ai->ai_family != AF_INET6)
			continue;
		
		s = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (s == -1)
			/* Maybe we can use another socket...*/
			continue;

		set_reuseaddr_sk(s);

		/* Let's bind it! */
		if(bind(s, ai->ai_addr, ai->ai_addrlen) < 0) {
			error("Cannot bind the port %d: %s. Trying another "
					"socket...", port, strerror(errno));
			close(s);
			continue;
		}
		freeaddrinfo(aitop);
		return s;
	}
	
	error("Cannot open inbound socket on port %d: %s", port, strerror(errno));
	freeaddrinfo(aitop);
	return -1;
}

/*
 *  udp_exec_pkt: passes the received udp packet to pkt_exec().
 * `passed_argv' is a pointer to a udp_exec_pkt_argv struct 
 */
void *udp_exec_pkt(void *passed_argv)
{
	struct udp_exec_pkt_argv argv;
	
	PACKET rpkt;
	const char *ntop;

	memcpy(&argv, passed_argv, sizeof(struct udp_exec_pkt_argv));
	memcpy(&rpkt, argv.recv_pkt, sizeof(PACKET));

	if(argv.flags & UDP_THREAD_FOR_EACH_PKT)
		xfree(argv.recv_pkt);
	
	/* Drop any packet we sent in broadcast */
	if(!memcmp(&rpkt.from, &me.cur_ip, sizeof(inet_prefix))) {
		pkt_free(&rpkt, 0);
		return 0;
	}

	if(add_accept(rpkt.from, 1)) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NORMAL, "ACPT: dropped UDP pkt from %s: "
				"Accept table full.", ntop);
		return 0;
	} 

	pkt_exec(rpkt, argv.acpt_idx);
	pkt_free(&rpkt, 0);
	
	return 0;
}

/*
 * udp_daemon: Takes care of receiving udp packets.
 * `passed_argv' is a pointer to a udp_daemon_argv struct
 */
void *udp_daemon(void *passed_argv)
{
	struct udp_daemon_argv argv;
	struct udp_exec_pkt_argv exec_pkt_argv;
	PACKET rpkt;
	fd_set fdset;
	int ret, sk;
	u_short udp_port;
	PACKET *rpkt_cp;
	pthread_t thread;
	pthread_attr_t t_attr;
#ifdef DEBUG
	int select_errors=0;
#endif
	
	memcpy(&argv, passed_argv, sizeof(struct udp_daemon_argv));
	udp_port=argv.port;
	memset(&exec_pkt_argv, 0, sizeof(struct udp_exec_pkt_argv));

	if(argv.flags & UDP_THREAD_FOR_EACH_PKT) {
		pthread_attr_init(&t_attr);
		pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
		exec_pkt_argv.flags|=UDP_THREAD_FOR_EACH_PKT;
	}

	debug(DBG_SOFT, "Preparing the udp listening socket on port %d", udp_port);
	sk=prepare_listen_socket(my_family, SOCK_DGRAM, udp_port);
	if(sk == -1)
		return NULL;
	set_bindtodevice_sk(sk, me.cur_dev);
	/* set_broadcast_sk(sk, my_family, me.cur_dev_idx); */
	
	debug(DBG_NORMAL, "Udp daemon on port %d up & running", udp_port);
	pthread_mutex_unlock(&udp_daemon_lock);
	for(;;) {
		if(!sk)
			fatal("The udp_daemon socket got corrupted");
		
		FD_ZERO(&fdset);
		FD_SET(sk, &fdset);
		
		ret = select(sk+1, &fdset, NULL, NULL, NULL);
		if (ret < 0) {
#ifdef DEBUG
			if(select_errors > 20)
				break;
			select_errors++;
#endif
			error("daemon_udp: select error: %s", strerror(errno));
			continue;
		}
		if(!FD_ISSET(sk, &fdset))
			continue;

		memset(&rpkt, 0, sizeof(PACKET));
		pkt_addsk(&rpkt, my_family, sk, SKT_UDP);
		pkt_addflags(&rpkt, MSG_WAITALL);
		pkt_addport(&rpkt, udp_port);

		if(pkt_recv(&rpkt) < 0) {
			pkt_free(&rpkt, 0);
			continue;
		}
	
		exec_pkt_argv.acpt_idx=accept_idx;
		exec_pkt_argv.acpt_sidx=accept_sidx;

		if(argv.flags & UDP_THREAD_FOR_EACH_PKT) {
			rpkt_cp=xmalloc(sizeof(PACKET));
			memcpy(rpkt_cp, &rpkt, sizeof(PACKET));
			exec_pkt_argv.recv_pkt=rpkt_cp;
			pthread_create(&thread, &t_attr, udp_exec_pkt,
					&exec_pkt_argv);
		} else {
			exec_pkt_argv.recv_pkt=&rpkt;
			udp_exec_pkt(&exec_pkt_argv);
		}
	}

	destroy_accept_tbl();
	return NULL;
}

void *tcp_recv_loop(void *recv_pkt)
{
	PACKET rpkt;
	int acpt_idx, acpt_sidx;

	acpt_idx=accept_idx;
	acpt_sidx=accept_sidx;
	memcpy(&rpkt, recv_pkt, sizeof(PACKET));
	xfree(recv_pkt);

	add_accept_pid(getpid(), acpt_idx, acpt_sidx);

	while( pkt_recv(&rpkt) != -1 ) {
		if(pkt_exec(rpkt, acpt_idx) < 0) {
			goto close;
			break;
		} else
			pkt_free(&rpkt, 0);
	}

close:
	pkt_free(&rpkt, 1);
	close_accept(acpt_idx, acpt_sidx);

	return NULL;
}

void *tcp_daemon(void *door)
{
	pthread_t thread;
	pthread_attr_t t_attr;
	PACKET rpkt;
	struct sockaddr_storage addr;
	socklen_t addrlen = sizeof addr;
	inet_prefix ip;
	fd_set fdset;
	int sk, fd, ret, err;
	u_short tcp_port=*(u_short *)door;
	const char *ntop;
	char *rpkt_cp;

	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);
	
	debug(DBG_SOFT, "Preparing the tcp listening socket on port %d", tcp_port);
	sk=prepare_listen_socket(my_family, SOCK_STREAM, tcp_port);
	if(sk == -1)
		return NULL;
	set_bindtodevice_sk(sk, me.cur_dev);

	/* 
	 * While we are accepting the connections we keep the socket non
	 * blocking.
	 */
	if(set_nonblock_sk(sk))
		return NULL;

	/* Shhh, it's listening... */
	if(listen(sk, 5) == -1) {
		close(sk);
		return NULL;
	}
	
	debug(DBG_NORMAL, "Tcp daemon on port %d up & running", tcp_port);
	pthread_mutex_unlock(&tcp_daemon_lock);
	for(;;) {
		if(!sk)
			fatal("The tcp_daemon socket got corrupted");
		
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

		memset(&rpkt, 0, sizeof(PACKET));
		pkt_addsk(&rpkt, my_family, fd, SKT_TCP);
		pkt_addflags(&rpkt, MSG_WAITALL);
		pkt_addport(&rpkt, tcp_port);

		ntop=0;
		sockaddr_to_inet((struct sockaddr *)&addr, &ip, 0);
		pkt_addfrom(&rpkt, &ip);
		if(server_opt.dbg_lvl)
			ntop=inet_to_str(ip);

		if((ret=add_accept(ip, 0))) {
			debug(DBG_NORMAL, "ACPT: drop connection with %s: "
					"Accept table full.", ntop);
			
			/* Omg, we cannot take it anymore, go away: ACK_NEGATIVE */
			pkt_err(rpkt, ret);
			close(fd);
			continue;
		} else {
			debug(DBG_NORMAL, "ACPT: Accept_tbl ok! accept_idx: %d "
					"from %s", accept_idx, ntop);
			/* 
			 * Ok, the connection is good, send back the
			 * ACK_AFFERMATIVE.
			 */
			pkt_addto(&rpkt, &rpkt.from);
			send_rq(&rpkt, 0, ACK_AFFERMATIVE, 0, 0, 0, 0);
		}

		if(unset_nonblock_sk(fd))
			continue;
	

		rpkt_cp=xmalloc(sizeof(PACKET));
		memcpy(rpkt_cp, &rpkt, sizeof(PACKET));
		err=pthread_create(&thread, &t_attr, tcp_recv_loop, (void *)rpkt_cp);
		pthread_detach(thread);
	}
	
	destroy_accept_tbl();
	return NULL;
}
