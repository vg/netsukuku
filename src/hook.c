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
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ll_map.c"
#include "map.h"
#include "inet.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

extern struct current me;
extern int my_family;

int get_free_ips(inet_prefix to, struct free_ips *fi_hdr, int *ips)
{
	PACKET pkt, rpkt;
	char *ntop;
	ssize_t err;

	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	debug(DBG_NORMAL, "Sending the GET_FREE_IPS request to %s", ntop);

	/* * * the GET_FREE_IPS request * * */
	pkt_fill_hdr(&pkt.hdr, 0, GET_FREE_IPS, 0);
	pkt.msg=0;
	to.family=my_family;
	pkt_addto(&pkt, &to);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addport(&rpkt, ntk_tcp_port);
	pkt_addflags(&pkt, NULL);
	if((rpkt.sk=pkt.sk=new_tcp_conn(pkt.to, ntk_udp_port))==-1) {
		error("get_free_ips(): Couldn't connect to %s to launch the GET_FREE_IPS request", ntop);
		return -1;
	}
	/*Let's send the request*/
	err=pkt_send(pkt);
	pkt_free(&pkt);
	if(err==-1) {
		error("get_free_ips(): Cannot send the GET_FREE_IPS request to %s. Skipping...", ntop);
		return -1;
	}

	/* * * the reply * * */
	pkt_recv(&rpkt);
	if(pkt.hdr.op==ACK_NEGATIVE) {
		int err_ack;
		char *n;
		
		memcpy(&err_ack, pkt.buf, pkt.hdr.sz);
		n=inet_to_str(&to);
		error("GET_FREE_IPS failed. The node %s replied: %s", n, rq_strerror(err_ack));
		xfree(n);
		pkt_free(&pkt);
		return -1;
	}

	memcpy(fi_hdr, pkt.msg, sizeof(struct free_ips));
	if(fi_hdr->ips <= 0 || fi_hdr->ips >= MAXGROUPNODE) {
		error("Malformed PUT_FREE_IPS request hdr. It says there are %d free ip", fi_hdr->ips);
		pkt_free(&pkt);
                return -1;
	}
	memcpy(ips, pkt.msg+sizeof(struct free_ips), fi_hdr->ips*sizeof(int));
	
	debug(DBG_NORMAL, "received %d free ips", fi_hdr->ips);
	
	pkt_free(&rpkt);
        xfree(ntop);
	return 0;
}

int put_free_ips(PACKET rq_pkt)
{
	/*I'm using this temp struct to do pkt.msg=&fipkt; see below*/
	struct fi_pkt {
		struct free_ips fi_hdr;
		int free_ips[MAXGROUPNODE];
	}fipkt;
	PACKET pkt;
	map_node *map=me.int_map;
	
	char *ntop; 
	ssize_t err, list_sz, i, e=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_FREE_IPS reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	to.family=my_family;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, NULL);
	if((pkt.sk=new_tcp_conn(pkt.to, ntk_udp_port))==-1) {
		error("put_free_ips(): Couldn't connect to %s to launch the PUT_FREE_IPS reply", ntop);
		return -1;
	}

	if(me.cur_gnode.flags & GMAP_FULL) {
		/*<<My gnode is full, sry>>*/
		pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_IPS, 0);
		err=pkt_err(pkt, E_GNODE_FULL);
	} else {
		fi_hdr.gid=me.cur_gid;
		memcpy(&fi_hdr.ipstart, &me.ipstart, sizeof(inet_prefix));
		/*Creates the list of our gnode's free ips. (it scans the int_map to find all the 
		 * MAP_VOID nodes)*/
		for(i=0; i<MAXGROUPNODE; i++)
			if(map[i].flags & MAP_VOID) {
				free_ips[e]=map[i]-me.int_map;
				e++:
			}

		fi_hdr.ips=e;
		list_sz=e*sizeof(inet_prefix);
		pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_IPS, list_sz);
		pkt.msg=&fipkt;
		err=pkt_send(pkt);
	}
	
	pkt_free(&pkt);
	if(err==-1) {
		error("put_free_ips(): Cannot send the PUT_FREE_IPS reply to %s. Skipping...", ntop);
		return -1;
	}
	
	xfree(ntop);
	return 0;
}


int netsukuku_hook(char *dev)
{
	int i, e=0, idx, imaps=0;
	u_int idata[4];
	struct free_ips fi_hdr;
	int fips[MAXGROUPNODE];
	map_node **merg_map;

	debug(DBG_NOISE, "Starting the hooking");
	if(my_family==AF_INET)
		idata[0]=HOOKING_IP;
	else
		idata[0]=HOOKING_IP6;
	me.cur_ip.family=my_family;
	inet_setip(&me.cur_ip, idata, my_family);
	memcpy(&me.cur_ip, &me.cur_ip);
	
	/*We set the dev ip to HOOKING_IP to begin our transaction.*/
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the HOOKING_IP in %s", ERROR_POS, dev);
	
	if(route_add_df_gw(dev))
		fatal("%s:%d: Couldn't set the default gw for %s", ERROR_POS, dev);
	
	debug(DBG_NORMAL, "The hook begins. Starting to scan the area");
	cur_node->flags|=MAP_HNODE;
	
	/* We do our first scan to know what we've around us. The rnodes are kept in
	 * me.cur_node->r_nodes. The fastest one is in me.cur_node->r_nodes[0]
	 */
	if(radar_scan())
		fatal("%s:%d: Scan of the area failed. Cannot continue.", ERROR_POS);
	
	if(!me.cur_node->links)
		fatal("%s:%d: No nodes founded! This is a black zone. Go somewhere else", ERROR_POS);

	
	/*Now we choose the nearest rnode we found and we send it the GET_FREE_IPS request*/
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");

		if(!get_free_ips(radar_q[idx].ip, &fi_hdr, fips)) {
			e=1;
			break;
		}
	}	
	if(!e)
		fatal("None of the nodes in this area gave me the free_ips info");

	
	/*Now we choose a random ip from the free ips we received*/
	e=(rand()%(fi_hdr.ips-0+1))+0;		/*rnd_range algo: (rand()%(max-min+1))+min*/
	maptoip(0, fips[e], fi_hdr.ipstart, &me.cur_ip);
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the tmp_ip in %s", ERROR_POS, dev);

	/*Fetch the ext_map from the nearest rnode*/
	e=0;
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");
		
		if(!get_ext_map(radar_q[idx].ip, &me.ext_map)) {
			e=1;
			break;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the extern map");
	
	/*Fetch the int_map from each rnode*/
	e=0;
	merg_map=xmalloc(me.cur_node->links*sizeof(map_node *));
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");

		if(!get_int_map(radar_q[idx].ip, merg_map[e])) {
			*(merg_map+i)=xmalloc(sizeof(map_node)*MAXGROUPNODE);
			e=++; imaps++;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the intern map");

	int_map=merge_int_maps(int_maps);
	for(i=0, i<imaps; i++)
		xfree(merg_map[i]);
	xfree(merg_map);
	
	/* We must reset the radar_queue because the first radar_scan used while hooking
	 * has to keep the list of the rnodes' "inet_prefix ip". In this way we know
	 * the rnodes' ips even if we haven't an int_map yet.
	 */
	reset_radar(me.cur_node->links);

	/*TODO: for now the gnodes aren't supported
	 * djkstra(ext_map);
	 */
	dnode_set_ip(rnd(info.free_ips);
	QSPN_SEND_DNODE_GATHERING();
	
	thread(snode_transform());

	return;
}

int snode_transfrom()
{
	wait(DNODE_TO_SNODE_WAIT);
	local_broadcast_send(I_AM_A_SNODE);
	rcv(OK_FROM_RNODES);
	QSPN_SEND();
}
