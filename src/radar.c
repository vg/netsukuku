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

#include "llist.c"
#include "inet.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "pkts.h"
#include "qspn.h"
#include "radar.h"
#include "request.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"

extern struct current me;
extern int my_family;

void init_radar(void)
{
	radar_scans=0;
	my_echo_id=0;
	max_radar_wait=MAX_RADAR_WAIT;	
	list_init(radar_q);
	memset(radar_q, 0, sizeof(struct radar_queue));
	memset(send_qspn_now, 0, sizeof(int)*MAX_LEVELS);
	radar_scan_mutex=0;
}


void close_radar(void)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_destroy(radar_q);
	radar_q=0;
}

void reset_radar(void)
{
	close_radar();
	init_radar();
}

void free_new_node(void)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq)
		xfree(rq->node);
}

struct radar_queue *find_ip_radar_q(map_node *node)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq) {
		if(rq->node==node)
			return rq;
	}
	return 0;
}

map_node *find_nnode_radar_q(inet_prefix *node)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq) {
		if(!memcmp(&rq->ip, node, sizeof(inet_prefix)))
			return rq->node;
	}
	return 0;
}


void final_radar_queue(void)
{	
	struct radar_queue *rq;
	int e;
	struct timeval sum;
	u_int f_rtt;

	memset(&sum, '\0', sizeof(struct timeval));

	rq=radar_q;
	list_for(rq) {
		if(!rq->node)
			continue;
		for(e=0; e<rq->pongs; e++) {
			timeradd(&rq->rtt[e], &sum, &sum);
		}
		
		f_rtt=MILLISEC(sum)/radar_scans;
		rq->final_rtt.tv_sec=f_rtt/1000;
		rq->final_rtt.tv_usec=(f_rtt - (f_rtt/1000)*1000)*1000;
	}
	
	my_echo_id=0;
}

/* 
 * radar_remove_old_rnodes: It removes all the old rnodes ^_- It store in 
 * rnode_delete[level] the number of deleted rnodes. This function is used 
 * by radar_update_map
 */
int radar_remove_old_rnodes(int *rnode_deleted) 
{
	map_node *node, *root_node;
	ext_rnode *e_rnode;
	struct qspn_buffer *qb;
	int i, node_pos, bm, rnode_pos;
	u_char level, external_node, total_levels;
	void *void_map;

	for(i=0; i<me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;

		if(!(node->flags & MAP_VOID))
			continue;
		/* Doh, The rnode is dead! */

		if(node->flags & MAP_ERNODE) {
			e_rnode=(ext_rnode *)node;
			external_node=1;
			total_levels=e_rnode->quadg.levels;
		} else {
			external_node=0;
			total_levels=1;
		}

		for(level=0; level < total_levels; level++) {
			qspn_set_map_vars(level, 0, &root_node, 0, 0);
			qb=qspn_b[level];

			if(!level && external_node) {
				debug(DBG_NORMAL,"The external node of gid %d is dead\n", e_rnode->quadg.gid[1]);
			} else if(!level) {
				void_map=me.int_map;
				node_pos=pos_from_node(node, me.int_map);
				debug(DBG_NORMAL,"The node %d is dead\n", node_pos, me.int_map);
			} else
				void_map=me.ext_map;

			/* 
			 * We don't care to send the qspn to inform the other nodes of this death. 
			 * They will wait till the next qspn round to know it.
			 * send_qspn_now[level]=1;
			 */
			rnode_del(root_node, i);
			if(!external_node)
				map_node_del((map_node *)me.cur_node->r_node[i].r_node);
			else {
				gmap_node_del(e_rnode->quadg.gnode[_EL(level)]);

				/* bnode_map update */
				bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], void_map, 
						(void *)root_node, level);
				if(bm!=-1) {
					rnode_pos=rnode_find(&me.bnode_map[level][bm], 
							(map_node *) e_rnode->quadg.gnode[_EL(level+1)]);
					if(rnode_pos!=-1)
						rnode_del(&me.bnode_map[level][bm], rnode_pos);
				}

				list_del(e_rnode);
			}
			/* Now we delete it from the qspn_buffer */
			list_for(qb)
				if(qb->rnode == node)
					list_del(qb);
			rnode_deleted[level]++;
		}

	}
	return 0;
}

/* 
 * radar_update_map: it updates the int_map and the ext_map if any bnodes are found.
 * Note that the rnodes in the map are held in a different way. First of all the qspn
 * is not applied to them (we already know how to reach them ;) and they have only
 * one rnode... ME. So me.cur_node->r_node[x].r_node->r_node[0] == me.cur_node.
 * Gotcha?
 */
void radar_update_map(void)
{
	struct radar_queue *rq;
	map_node *root_node, *node;
	map_rnode rnn, *rnode, rn;
	ext_rnode *e_rnode;
	ext_rnode_cache *erc;
	int i, e, diff, bm;
	int rnode_added[MAX_LEVELS], rnode_deleted[MAX_LEVELS], rnode_rtt, rnode_pos, node_pos;
	u_char level, external_node, total_levels;
	void *void_map;

	memset(rnode_added, 0, sizeof(int)*MAX_LEVELS);
	memset(rnode_deleted, 0, sizeof(int)*MAX_LEVELS);
	
	/*
	 * Let's consider all our rnodes void, in this way we'll know what
	 * rnodes will remain void after the update.
	 */
	for(i=0; i<me.cur_node->links; i++) {
		if(node->flags & MAP_GNODE) {
			e_rnode=(ext_rnode *)me.cur_node->r_node[i].r_node;
			for(e=1; e<e_rnode->quadg.levels; e++) {
				node=&e_rnode->quadg.gnode[_EL(level)]->g;
				node->flags|=MAP_VOID | MAP_UPDATE;
			}
		} else {
			node=(map_node *)me.cur_node->r_node[i].r_node;
			node->flags|=MAP_VOID | MAP_UPDATE;
		}
	}

	rq=radar_q;
	list_for(rq) {
	           if(rq->node == RADQ_VOID_RNODE)
			   continue;
		 
		   /* We need to know if it is a node which is not in the gnode where we are.*/
		   if((int)rq->node == RADQ_EXT_RNODE) {
			   external_node=1;
			   total_levels=rq->quadg.levels;
		   } else {
			   external_node=0;
			   total_levels=1;
		   }

		   for(level=0; level < total_levels; level++) {
			   if(!level) {
				   root_node=me.cur_node;
				   void_map=me.int_map;
				   node=rq->node;
			   } else {
				   root_node=&me.cur_quadg.gnode[_EL(level)]->g;
				   void_map=me.ext_map;
				   node=&rq->quadg.gnode[_EL(level)]->g;
			   }
			   if(external_node)
				   rnode_pos=e_rnode_find(me.cur_erc, &rq->quadg);
			   else
				   rnode_pos=rnode_find(root_node, node);

			   if(rnode_pos == -1) { /* W00t, we've found a new rnode! */
				   struct qspn_buffer *qb;
				   rnode_pos=root_node->links; /* Now it is the last rnode +1 because we are adding it */

				   /* First of all we add it in the map... */
				   if(external_node && !level) {
					   /* 
					    * If this node we are processing is external, in the 
					    * root_node's rnodes we add a rnode which point to a
					    * ext_rnode struct
					    */

					   memset(&rnn, '\0', sizeof(map_rnode));
					   e_rnode=xmalloc(sizeof(ext_rnode));

					   memcpy(&e_rnode->quadg, &rq->quadg, sizeof(quadro_group));
					   e_rnode->node.flags=MAP_BNODE | MAP_GNODE | MAP_RNODE | MAP_ERNODE;
					   rnn.r_node=(u_int *)e_rnode;
					   node=rq->node=&e_rnode->node;
					   rnode=&rnn;
					  
					   /* Update the external_rnode_cache list */
					   erc=xmalloc(sizeof(ext_rnode_cache));
					   erc->e=e_rnode;
					   erc->rnode_pos=rnode_pos;
					   if(!me.cur_erc_counter)
						   list_init(me.cur_erc);
					   list_add(me.cur_erc, e_rnode);
					   me.cur_erc_counter++;					   
				   } else {
					   /* 
					    * We purge all the node's rnodes. We don't need anymore any
					    * qspn routes stored in it.
					    */
					   rnode_destroy(node);

					   node->flags|=MAP_BNODE | MAP_GNODE | MAP_RNODE;
					   root_node->flags|=MAP_BNODE;
					   rnode=&node->r_node[0];
				   }
				   rnode_add(root_node, rnode);

   				   /* ...and finally we update the qspn_buffer */
				   qb=xmalloc(sizeof(struct qspn_buffer));
				   memset(qb, 0, sizeof(struct qspn_buffer));
				   qb->rnode=node;
				   if(!root_node->links)
					   list_init(qspn_b[level]);
				   list_add(qspn_b[level], qb);

				   rnode_added[level]++;
				   send_qspn_now[level]=1;
			   } else {
				   if(external_node)
					   node=(map_node *)root_node->r_node[rnode_pos].r_node;
				   /* Nah, We have already it. Let's just update its rtt */
				   if(!send_qspn_now[level] && node->links) {
					   diff=abs(MILLISEC(root_node->r_node[rnode_pos].rtt) - MILLISEC(rq->final_rtt));
					   if(diff >= RTT_DELTA)
						   send_qspn_now[level]=1;
				   }
			   }
			   node->flags&=~MAP_VOID & ~MAP_UPDATE;
		           memcpy(&root_node->r_node[rnode_pos].rtt, &rq->final_rtt, sizeof(struct timeval));
			   
			   /* There's nothing more better than updating the bnode_map. */
			   if(external_node && level <= GET_LEVELS(my_family)) {
				   bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], void_map, 
						   (void *)root_node, level);
				   if(bm==-1)
					   bm=map_add_bnode(&me.bnode_map[level], &me.bmap_nodes[level], 
							   (u_int)root_node, 1);
				   rnode_pos=rnode_find(&me.bnode_map[level][bm], 
						   (map_node *)rq->quadg.gnode[_EL(level+1)]);
				   if(rnode_pos == -1) {
					   memset(&rn, 0, sizeof(map_rnode));
					   rn.r_node=(u_int *)rq->quadg.gnode[_EL(level+1)];
					   rnode_add(&me.bnode_map[level][bm], &rn);
					   rnode_pos=me.bnode_map[level][bm].links-1;
				   }
				   rnode=&me.bnode_map[level][bm].r_node[rnode_pos];
				   memcpy(&rnode->rtt, &rq->final_rtt, sizeof(struct timeval));
			   }
		   } /*for(level=0, ...)*/
	} /*list_for(rq)*/

	radar_remove_old_rnodes(rnode_deleted);

	/* My mom always says: <<keep your room tidy... order, ORDER>> */
	if(rnode_added[0] || rnode_deleted[0])
		rnode_rtt_order(me.cur_node);
	for(i=1; i<me.cur_quadg.levels; i++)
		if(rnode_added[i] || rnode_deleted[i])
			rnode_rtt_order(&me.cur_quadg.gnode[_EL(level)]->g);
}

int add_radar_q(PACKET pkt)
{
	map_node *rnode;
	quadro_group quadg;
	struct timeval t;
	struct radar_queue *rq;
	u_int idx, ret;
	
	gettimeofday(&t, 0);
	
	if(me.cur_node->flags & MAP_HNODE) {
		/* 
		 * We are hooking, we haven't yet an int_map, an ext_map,
		 * a stable ip so we create fake nodes that will delete after
		 * the hook.
		 */
		if(!(rnode=find_nnode_radar_q(&pkt.from))) {
			map_rnode rnn;

			rnode=xmalloc(sizeof(map_node));
			memset(rnode, '\0', sizeof(map_node));
			memset(&rnn, '\0', sizeof(map_rnode));

			rnn.r_node=(u_int *)me.cur_node;
			rnode_add(rnode, &rnn);
		}
	} else
		ret=iptomap((int)me.int_map, pkt.from, me.cur_quadg.ipstart[1],
				(u_int *)&rnode);
	
	iptoquadg(pkt.from, me.ext_map, &quadg, QUADG_GID | QUADG_GNODE | QUADG_IPSTART);
	
	if(!(rq=find_ip_radar_q(rnode))) {
		rq=xmalloc(sizeof(struct radar_queue));
		memset(rq, 0, sizeof(struct radar_queue));
		list_add(radar_q, rq);
	}

	if(ret)
		rq->node=(map_node *)RADQ_EXT_RNODE;
	else
		rq->node=rnode;
	memcpy(&rq->ip, &pkt.from, sizeof(inet_prefix));
	memcpy(&rq->quadg, &quadg, sizeof(quadro_group));

	if(rq->pongs<=radar_scans) {
		timersub(&t, &scan_start, &rq->rtt[rq->pongs]);
		/* 
		 * Now we divide the rtt, because (t - scan_start) is the time
		 * the pkt used to reach B from A and to return to A from B
		 */
		rq->rtt[rq->pongs].tv_sec/=2;
		rq->rtt[rq->pongs].tv_usec/=2;
		rq->pongs++;
	}

	return 0;
}
	

int radar_recv_reply(PACKET pkt)
{
	int i, e=0;

	if(!my_echo_id) {
		loginfo("I received an ECHO_REPLY with id: %d, but I've never "
				"sent any ECHO_ME requests..", pkt.hdr.id);
		return -1;
	}
	
	if(pkt.hdr.id != my_echo_id) {
		loginfo("I received an ECHO_REPLY with id: %d, but I've never "
				"sent an ECHO_ME with that id!", pkt.hdr.id);
		return -1;
	}

	return add_radar_q(pkt);
}

		            
/* 
 * radar_scan: It starts the scan of the local area.
 * It sends MAX_RADAR_SCANS packets in broadcast then it waits MAX_RADAR_WAIT
 * and in the while the echo replies are gathered. After MAX_RADAR_WAIT it 
 * stops to receive echo replies and it does a statistical analysis of the 
 * gathered echo replies, it updates the r_nodes in the map and sends a qspn 
 * round if something is changed in the map.
 * It returns 1 if another radar_scan is in progress, -1 if something went
 * wrong, 0 on success.
 */
int radar_scan(void) 
{
	PACKET pkt;
	inet_prefix broadcast;
	int i, e=0;
	ssize_t err;		

	/* We are already doing a radar scan, that's not good */
	if(radar_scan_mutex)
		return 1;
	radar_scan_mutex=1;	
	
	/* We create the PACKET */
	memset(&pkt, '\0', sizeof(PACKET));
	broadcast.family=my_family;
	inet_setip_bcast(&broadcast);
	pkt_addto(&pkt, &broadcast);
	pkt.sk_type=SKT_BCAST;
	my_echo_id=random();
	for(i=0; i<MAX_RADAR_SCANS; i++) {
		err=send_rq(&pkt, 0, ECHO_ME, my_echo_id, 0, 0, 0);
		if(err==-1) {
			error("radar_scan(): Error while sending the scan %d"
					"... skipping", my_echo_id);
			continue;
		}
		radar_scans++;
	}
	if(!radar_scans) {
		error("radar_scan(): The scan (%d) faild. It wasn't possible" 
				"to send a single scan", my_echo_id);
		return -1;
	}
	pkt_free(&pkt, 1);
	
	gettimeofday(&scan_start, 0);
	sleep(max_radar_wait);

	final_radar_queue();
	radar_update_map();
	if(!(me.cur_node->flags & MAP_HNODE)) {
		for(i=0; i<me.cur_quadg.levels; i++)
			if(send_qspn_now[i])
				/* We start a new qspn_round in the level `i' */
				qspn_send(i);
		reset_radar();
	} else
		free_new_node();

	radar_scan_mutex=0;	
	return 0;
}

/* 
 * radard: It sends back via broadcast the ECHO_REPLY to the ECHO_ME pkt received
 */
int radard(PACKET rpkt)
{
	PACKET pkt;
	inet_prefix broadcast;
	ssize_t err;

	/* We create the PACKET */
	memset(&pkt, '\0', sizeof(PACKET));
	
	broadcast.family=my_family;
	inet_setip_bcast(&broadcast);
	pkt_addto(&pkt, &broadcast);
	pkt.sk_type=SKT_BCAST;
	
	/* We send it */
	err=send_rq(&pkt, 0, ECHO_REPLY, rpkt.hdr.id, 0, 0, 0);
	pkt_free(&pkt, 1);
	if(err==-1) {
		char *ntop;
		ntop=inet_to_str(&pkt.to);
		error("radard(): Cannot send back the ECHO_REPLY to %s.", ntop);
		xfree(ntop);
		return -1;
	}
	return 0;
}

void *radar_daemon(void *null)
{
	for(;;) {
		radar_scan();
		sleep(MAX_RADAR_WAIT+2);
	}
}

