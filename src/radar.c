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

#include <string.h>

#include "radar.h"
#include "netsukuku.h"
#include "inet.h"
#include "pkts.h"
#include "xmalloc.h"
#include "log.h"

extern struct current me;
extern int my_family;

void init_radar(void)
{
	int i;
	radar_q=xmalloc(10*sizeof(struct radar_queue));
	memset(radar_q, '\0', 10*sizeof(struct radar_queue));
	radar_q_alloc=10;
	radar_scan_mutex=0;
}

void reset_radar(int alloc)
{
	memset(radar_q, '\0', radar_q_alloc*sizeof(struct radar_queue));
	radar_q=xrealloc(radar_q, alloc);
	radar_q_alloc=alloc;
}

void close_radar(void)
{
	xfree(radar_q);
	radar_q_alloc=0;
}

void free_new_node(void)
{
	int i;
	for(i=0; i<radar_q_alloc; i++)
		xfree(radar_q[i].node);
}

int find_free_radar_q(void)
{
	int i;

	for(i=0; i<radar_q_alloc; i++) {
		if(!radar_q[i].node)
			return i;
	}
	return -1;
}
		
int find_ip_radar_q(map_node *node)
{
	int i;

	for(i=0; i<radar_q_alloc; i++) {
		if(radar_q[i].node==node)
			return i;
	}
	return -1;
}

u_int *find_nnode_radar_q(inet_prefix *node)
{
	int i;

	for(i=0; i<radar_q_alloc; i++) {
		if(!memcmp(&radar_q[i].ip, node, sizeof(inet_prefix)))
			return radar_q[i].node;
	}
	return 0;
}


void final_radar_queue(void)
{	
	int i, e;
	struct timeval sum;
	u_int f_rtt;

	memset(&sum, '\0', sizeof(struct timeval));

	for(i=0; i<radar_q_alloc; i++) {
		if(!radar_q[i].node)
			continue;
		for(e=0; e<radar_q[i].pongs; e++) {
			timeradd(radar_q[i].rtt[e], &sum, &sum);
		}
		
		f_rtt=MILLISEC(sum)/radar_scans;
		radar_q[i].final_rtt.tv_sec=f_rtt/1000;
		radar_q[i].final_rtt.tv_usec=(f_rtt - (f_rtt/1000)*1000)*1000;
		
	}
	
	my_echo_id=0;
}

void radar_update_map(void)
{
	int i, diff, rnode_added=0, rnode_deleted=0;

	/*Let's consider all our rnodes void, in this way we'll know what
	 * rnodes will remain void.*/
	for(i=0; i<me.cur_node->links; i++)
		me.cur_node->r_node[i].r_node->flags|=MAP_VOID | MAP_UPDATE;

	for(i=0; i<radar_q_alloc; i++) {
	           if(!radar_q[i].node)
			   continue;
		  
		   if(!send_qspn_now && radar_q[i].node->links) {
			   diff=abs(MILLISEC(radar_q[i].node->r_node[0].rtt) - MILLISEC(radar_q[i].final_rtt));
			   if(diff >= RTT_DELTA)
				   send_qspn_now=1;
		   }
		   
		   memcpy(&radar_q[i].node->r_node[0].rtt, &radar_q[i].final_rtt, sizeof(struct timeval));
		   radar_q[i].node->flags&=~MAP_VOID & ~MAP_UPDATE;
		
		   /*W00t, We've found a new rnode!*/
		   if(!(radar_q[i].node->flags & MAP_RNODE)) {
		   	radar_q[i].node->flags|=MAP_RNODE;
			rnode_add(me.cur_node, radar_q[i].node.r_node);
			rnode_added++;		   
		   }
	}

	
	for(i=0; i<me.cur_node->links; i++) {
		if(me.cur_node->r_node[i].r_node->flags & MAP_VOID) {
			/*Doh, The rnode is dead!*/
			debug(DBG_NORMAL,"The node %d is dead\n", 
					((void *)me.cur_node->r_node[i].r_node-(void *)me.int_map)/sizeof(map_node));
			/* We don't care to send the qspn to inform the other nodes. They will wait till the next QSPN 
			 * to update their map.
			 * send_qspn_now=1;
			 */
			rnode_del(me.cur_node, i);
			rnode_deleted++;
		}
	}
	
	if(rnode_added || rnode_deleted)
		rnode_rtt_order(me.cur_node);
}

int add_radar_q(PACKET pkt)
{
	u_int idx;
	map_node *rnode;
	struct timeval t;
	
	gettimeofday(&t, 0);
	if(iptomap(me.int_map, pkt.from, me.cur_gnode->ipstart, rnode)) 
		if(!(me.cur_node->flags & MAP_HNODE)) {
			u_int *gmap;
			/*The pkt.ip isn't part of our gnode, thus we are a bnode.
			 * TODO: When the gnode support is complete: Here we have to add the 
			 * two border nodes (me) and the other in the gnode map
			 * gmap=GI2GMAP(me.ext_map, rnode);
			 */
			me.cur_node->flags|=MAP_BNODE;
			return 1;
		} else {
			/*We are hooking, so we haven't yet an int_map, an ext_map, a stable ip..*/
			if(!(rnode=find_nnode_radar_q(&pkt.from))) {
				map_rnode rnn;
				
				rnode=xmalloc(sizeof(map_node));
				memset(rnode, '\0', sizeof(map_node));
				memset(&rnn, '\0', sizeof(map_rnode));
				
				rnn.r_node=me.cur_node;
				rnode_add(rnode, &rnn);
			}
		}
			

	if((idx=find_ip_radar_q(rnode)==-1))
		if((idx=find_free_radar_q())==-1) {
			radar_q=xrealloc(radar_q, radar_q_alloc*2);
			memset(radar_q[radar_q_alloc+1], '\0', radar_q_alloc);
			radar_q_alloc*=2;
		}

	if(!radar_q[idx].node) {
			radar_q[idx].node=rnode;
			memcpy(&radar_q[idx].ip, &pkt.from, sizeof(inet_prefix));
	}

	if(radar_q[idx].pongs<=radar_scans) {
		timersub(&t, &scan_start, &radar_q[idx].rtt[radar_q[idx].pongs]);
		/* Now we divide the rtt, because (t - scan_start) is the time the pkt used to
		 * reach B from A and to return to A from B
		 */
		radar_q[idx].rtt[radar_q[idx].pongs].tv_sec/=2;
		radar_q[idx].rtt[radar_q[idx].pongs].tv_usec/=2;
		radar_q[idx].pongs++;
	}

	return 0;
}
	

int radar_recv_reply(PACKET pkt)
{
	int i, e=0;

	if(!my_echo_id) {
		loginfo("I received an ECHO_REPLY with id: %d, but I've never sent any ECHO_ME requests..", pkt.hdr.id);
		return -1;
	}
	
	if(pkt.hdr.id != my_echo_id) {
		loginfo("I received an ECHO_REPLY with id: %d, but I've never sent an ECHO_ME with that id!", pkt.hdr.id);
		return -1;
	}

	return add_radar_q(pkt);
}

		            
/* radar_scan: It starts the scan of the local area.
 * It sends MAX_RADAR_SCANS packets in broadcast then it waits MAX_RADAR_WAIT and in the while 
 * the echo replies are gathered. After MAX_RADAR_WAIT it stops to receive echo replies and it 
 * does a statistical analysis of the gathered echo replies, it updates the r_nodes in the map
 * and sends a qspn round if something is changed in the map.
 * It returns 1 if another radar_scan is in progress, -1 if something went wrong, 0 on success.
 */
int radar_scan(void) 
{
	PACKET pkt;
	inet_prefix broadcast;
	int i, e=0;
	ssize_t err;		

	/*We are already doing a radar scan, that's not good*/
	if(radar_scan_mutex)
		return 1;
	radar_scan_mutex=1;	
	
	/*We create the PACKET*/
	memset(&pkt, '\0', sizeof(PACKET));
	broadcast.family=my_family;
	inet_setip_bcast(&broadcast);
	pkt_addto(&pkt, &broadcast);
	pkt.sk_type=SKT_BCAST;
	my_echo_id=random():
	for(i=0; i<MAX_RADAR_SCANS; i++) {
		err=send_rq(&pkt, 0, ECHO_ME, my_echo_id, 0, 0, 0);
		if(err==-1) {
			error("radar_scan(): Error while sending the scan %d... skipping", my_echo_id);
			continue;
		}
		radar_scans++;
	}
	if(!radar_scans) {
		error("radar_scan(): The scan (%d) faild. It wasn't possible to send a single scan", my_echo_id);
		return -1;
	}
	pkt_free(&pkt, 1);
	
	gettimeofday(&scan_start, 0);
	sleep(max_radar_wait);

	final_radar_queue();
	radar_update_map();
	if(!(me.cur_node->flags & MAP_HNODE)) {
		if(send_qspn_now)
	/*TODO*/	qspn_send(QSPN_CLOSE, NORMAL_BCAST);
		reset_radar(me.cur_node->links);
	}
	else
		free_new_node();

	radar_scan_mutex=0;	
	return 0;
}

/*radard: It sends back via broadcast the ECHO_REPLY to the ECHO_ME pkt received*/
int radard(PACKET rpkt)
{
	PACKET pkt;
	inet_prefix broadcast;
	ssize_t err;

	/*We create the PACKET*/
	memset(&pkt, '\0', sizeof(PACKET));
	
	broadcast.family=my_family;
	inet_setip_bcast(&broadcast);
	pkt_addto(&pkt, &broadcast);
	pkt.sk_type=SKT_BCAST;
	
	/*We send it*/
	err=send_rq(&pkt, 0, ECHO_REPLY, rpkt.hdr.id, 0, 0, 0);
	pkt_free(&pkt, 1);
	if(err==-1) {
		char *ntop;
		ntop=inet_to_str(&pkt->to);
		error("radard(): Cannot send back the ECHO_REPLY to %s.", ntop);
		xfree(ntop);
		return -1;
	}
	return 0;
}
