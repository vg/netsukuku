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

pthread_attr_t radar_qspn_send_t_attr;

void init_radar(void)
{
	hook_retry=0;
	my_echo_id=0;
	radar_scans=0;
	radar_scan_mutex=0;
	me.cur_erc_counter=0;
	max_radar_wait=MAX_RADAR_WAIT;	
	
	pthread_attr_init(&radar_qspn_send_t_attr);
	
	list_init(radar_q);
	list_init(me.cur_erc);
	
	memset(radar_q, 0, sizeof(struct radar_queue));
	memset(send_qspn_now, 0, sizeof(u_char)*MAX_LEVELS);
}


void close_radar(void)
{
	struct radar_queue *rq;
	rq=radar_q;
	
	list_destroy(radar_q);
	list_destroy(me.cur_erc);
	
	radar_q=0;
	me.cur_erc=0;
	me.cur_erc_counter=0;	

	pthread_attr_destroy(&radar_qspn_send_t_attr);
}

void reset_radar(void)
{
	if(me.cur_node->flags & MAP_HNODE)
		free_new_node();
	
	close_radar();
	init_radar();
}

void free_new_node(void)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq)
		if(rq->node && ((int)rq->node != RADQ_EXT_RNODE))
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
	list_for(rq)
		if(!memcmp(&rq->ip, node, sizeof(inet_prefix)))
			return rq->node;
		
	return 0;
}

int count_hooking_nodes(void) 
{
	struct radar_queue *rq;
	int total_hooking_nodes=0;

	rq=radar_q;
	list_for(rq) {
		if(!rq->node)
			continue;

		if(rq->node->flags & MAP_HNODE)
			total_hooking_nodes++;
	}
	
	return total_hooking_nodes;
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

		for(e=0; e<rq->pongs; e++)
			timeradd(&rq->rtt[e], &sum, &sum);

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
				debug(DBG_NORMAL, "The external node of gid %d is"
						" dead", e_rnode->quadg.gid[1]);
			} else if(!level) {
				void_map=me.int_map;
				node_pos=pos_from_node(node, me.int_map);
				debug(DBG_NORMAL, "The node %d is dead", 
						node_pos, me.int_map);
			} else
				void_map=me.ext_map;

			/*
			 * Just delete it from all the maps.
			 * We don't care to send the qspn to inform the other nodes of this death. 
			 * They will wait till the next qspn round to know it.
			 * send_qspn_now[level]=1;
			 */
			
			if(!external_node)
				map_node_del((map_node *)me.cur_node->r_node[i].r_node);
			else {
				gmap_node_del(e_rnode->quadg.gnode[_EL(level)]);

				/* bnode_map update */
				bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], 
						void_map, (void *)root_node, level);
				if(bm != -1) {
					rnode_pos=rnode_find(&me.bnode_map[level][bm], 
							(map_node *) e_rnode->quadg.gnode[_EL(level+1)]);
					if(rnode_pos != -1)
						rnode_del(&me.bnode_map[level][bm], rnode_pos);
				}

				list_del(e_rnode);
			}
			
			rnode_del(root_node, i);
			
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
	int rnode_added[MAX_LEVELS], rnode_deleted[MAX_LEVELS], rnode_pos;
	u_char level, external_node, total_levels;
	void *void_map;

	memset(rnode_added, 0, sizeof(int)*MAX_LEVELS);
	memset(rnode_deleted, 0, sizeof(int)*MAX_LEVELS);
	
	/*
	 * Let's consider all our rnodes void, in this way we'll know what
	 * rnodes will remain void after the update.
	 */
	for(i=0; i<me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;
		
		if(node->flags & MAP_GNODE) {
			e_rnode=(ext_rnode *)me.cur_node->r_node[i].r_node;
			for(e=1; e<e_rnode->quadg.levels; e++) {
				node=&e_rnode->quadg.gnode[_EL(level)]->g;
				node->flags|=MAP_VOID | MAP_UPDATE;
			}
		} else
			node->flags|=MAP_VOID | MAP_UPDATE;
	}

	rq=radar_q;
	list_for(rq) {
	           if(rq->node == RADQ_VOID_RNODE)
			   continue;
		   if(!(me.cur_node->flags & MAP_HNODE) && (rq->flags & MAP_HNODE))
			   continue;
		 
		   /* 
		    * We need to know if it is a node which is not in the gnode
		    * where we are.
		    */
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
				   char	*ntop;

				   ntop=inet_to_str(rq->ip);
				   loginfo("Radar: New node found: %s, ext: %d, level: %d", 
						   ntop, external_node, level);
				   xfree(ntop);
				   
				   rnode_pos=root_node->links; /* Now it is the last rnode +1
								  because we are adding it */

				   /* First of all we add it in the map... */
				   if(external_node && !level) {
					   /* 
					    * If this node we are processing is external, in the 
					    * root_node's rnodes we add a rnode which point to a
					    * ext_rnode struct
					    */

					   memset(&rnn, '\0', sizeof(map_rnode));
					   e_rnode=xmalloc(sizeof(ext_rnode));
					   memset(e_rnode, 0, sizeof(ext_rnode));

					   memcpy(&e_rnode->quadg, &rq->quadg, sizeof(quadro_group));
					   e_rnode->node.flags=MAP_BNODE | MAP_GNODE |
						   MAP_RNODE | MAP_ERNODE;
					   rnn.r_node=(u_int *)e_rnode;
					   node=rq->node=&e_rnode->node;
					   rnode=&rnn;
					  
					   /* Update the external_rnode_cache list */
					   erc=xmalloc(sizeof(ext_rnode_cache));
					   memset(erc, 0, sizeof(ext_rnode_cache));
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

					   /* 
					    * This node has only one rnodes, 
					    * and that is the root_node.
					    */
					   memset(&rnn, '\0', sizeof(map_rnode));
					   rnn.r_node=(u_int *)me.cur_node;
					   rnode_add(node, &rnn);

					   node->flags|=MAP_RNODE;
					   if(level) {
						   node->flags|=MAP_BNODE | MAP_GNODE;
						   root_node->flags|=MAP_BNODE;
					   }

					   memset(&rnn, '\0', sizeof(map_rnode));
					   rnn.r_node=(u_int *)node;					   
					   rnode=&rnn;
				   }

				   /* 
				    * The new node is added in the root_node's
				    * rnodes.
				    */
				   rnode_add(root_node, rnode);

   				   /* ...and finally we update the qspn_buffer */
				   qb=xmalloc(sizeof(struct qspn_buffer));
				   memset(qb, 0, sizeof(struct qspn_buffer));
				   qb->rnode=node;
				   if(root_node->links == 1)
					   list_init(qspn_b[level]);
				   list_add(qspn_b[level], qb);

				   rnode_added[level]++;
				   send_qspn_now[level]=1;
			   } else {
				   if(external_node)
					   node=(map_node *)root_node->r_node[rnode_pos].r_node;
				   /* 
				    * Nah, We have the node in the map. Let's just update its rtt 
				    */
				   if(!send_qspn_now[level] && node->links) {
					   diff=abs(MILLISEC(root_node->r_node[rnode_pos].rtt) -
							   MILLISEC(rq->final_rtt));
					   if(diff >= RTT_DELTA)
						   send_qspn_now[level]=1;
				   }
			   }
			   node->flags&=~MAP_VOID & ~MAP_UPDATE;
		           memcpy(&root_node->r_node[rnode_pos].rtt, &rq->final_rtt,
					   sizeof(struct timeval));
			   
			   /* 
			    * There's nothing better than updating the bnode_map. 
			    */
			   if(external_node && level <= GET_LEVELS(my_family)) {
				   bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level],
						   void_map, (void *)root_node, level);
				   if(bm==-1)
					   bm=map_add_bnode(&me.bnode_map[level], &me.bmap_nodes[level], 
							   (u_int)root_node, 0);
				   rnode_pos=rnode_find(&me.bnode_map[level][bm], 
						   (map_node *)rq->quadg.gnode[_EL(level+1)]);
				   if(rnode_pos == -1) {
					   memset(&rn, 0, sizeof(map_rnode));
					   rn.r_node=(u_int *)rq->quadg.gnode[_EL(level+1)];
					   rnode_add(&me.bnode_map[level][bm], &rn);
					   rnode_pos=0;
				   }
				   rnode=&me.bnode_map[level][bm].r_node[rnode_pos];
				   memcpy(&rnode->rtt, &rq->final_rtt, sizeof(struct timeval));
			   }
		   } /*for(level=0, ...)*/
	} /*list_for(rq)*/

	radar_remove_old_rnodes(rnode_deleted);

	/* <<keep your room tidy... order, ORDER>> */
	if(rnode_added[0] || rnode_deleted[0])
		rnode_rtt_order(me.cur_node);
	for(i=1; i<me.cur_quadg.levels; i++)
		if(rnode_added[i] || rnode_deleted[i])
			rnode_rtt_order(&me.cur_quadg.gnode[_EL(level)]->g);
}

/* 
 * add_radar_q: It returns the radar_q struct which handles the pkt.from node.
 * If the node is not present in the radar_q, it is added, and the
 * relative struct will be returned.
 */
struct radar_queue *
add_radar_q(PACKET pkt)
{
	map_node *rnode;
	quadro_group quadg;
	struct radar_queue *rq;
	u_int ret=0;
	
	if(me.cur_node->flags & MAP_HNODE) {
		/* 
		 * We are hooking, we haven't yet an int_map, an ext_map,
		 * a stable ip so we create fake nodes that will be delete after
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

		if(ret)
			rq->node=(map_node *)RADQ_EXT_RNODE;
		else {
			rq->node=rnode;
			/* 
			 * This pkt has been sent from another hooking
			 * node, let's remember this.
			 */
			if(pkt.hdr.flags & HOOK_PKT)
				rq->node->flags|=MAP_HNODE;

		}

		if(pkt.hdr.flags & HOOK_PKT)
			rq->flags|=MAP_HNODE;

		memcpy(&rq->ip, &pkt.from, sizeof(inet_prefix));
		memcpy(&rq->quadg, &quadg, sizeof(quadro_group));
	}

	return rq;
}

/* 
 * radar_exec_reply: It reads the received ECHO_REPLY pkt and updates the radar
 * queue, storing the calculated rtt and the other infos relative to the sender
 * node.
 */
int radar_exec_reply(PACKET pkt)
{
	struct timeval t;
	struct radar_queue *rq;
	u_int rtt_ms=0;
	
	gettimeofday(&t, 0);
	
	rq=add_radar_q(pkt);

	if(me.cur_node->flags & MAP_HNODE) {
		if(pkt.hdr.flags & HOOK_PKT) {
			u_char scanning;
			memcpy(&scanning, pkt.msg, sizeof(u_char));

			/* 
			 * If the pkt.from node has finished his scan, and we
			 * never received one of its ECHO_ME pkts, and we are
			 * still scanning, set the hook_retry.
			 */
			if(!scanning && !rq->pings && 
					(radar_scan_mutex ||
					 radar_scans<=MAX_RADAR_SCANS)) {
				hook_retry=1;
				debug(DBG_NOISE, "Hooking node ECHO_REPLY caught. s: %d,"
						" sca: %d, hook_retry: %d", radar_scans, 
						scanning, hook_retry);
			}
		}
	}

	if(rq->pongs < radar_scans) {
		timersub(&t, &scan_start, &rq->rtt[(int)rq->pongs]);
		/* 
		 * Now we divide the rtt, because (t - scan_start) is the time
		 * the pkt used to reach B from A and to return to A from B
		 */
		rtt_ms=MILLISEC(rq->rtt[(int)rq->pongs])/2;
		rq->rtt[(int)rq->pongs].tv_sec=rtt_ms/1000;
		rq->rtt[(int)rq->pongs].tv_usec=(rtt_ms - (rtt_ms/1000)*1000)*1000;

		rq->pongs++;
	}

	return 0;
}
	
/* 
 * radar_recv_reply: It handles the ECHO_REPLY pkts 
 */
int radar_recv_reply(PACKET pkt)
{
	if(!my_echo_id || !radar_scan_mutex || !radar_scans) {
		debug(DBG_NORMAL, "I received an ECHO_REPLY with id: 0x%x, but "
				"I've never sent any ECHO_ME requests..", 
				pkt.hdr.id);
		return -1;
	}
	
	if(pkt.hdr.id != my_echo_id) {
		debug(DBG_NORMAL,"I received an ECHO_REPLY with id: 0x%x, but "
				"I've never sent an ECHO_ME with that id!",
				pkt.hdr.id);
		return -1;
	}

	return radar_exec_reply(pkt);
}

/* 
 * radar_qspn_send_t: This function is used only by radar_scan().
 * It just call the qspn_send() function. We use a thread
 * because the qspn_send() may sleep, and we don't want to halt the
 * radar_scan().
 */
void *radar_qspn_send_t(void *level)
{
	int *p;
	u_char i;

	p=(int *)level;
	i=(u_char)*p;
	xfree(p);
	
	qspn_send(i);

	return NULL;
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
	pthread_t thread;
	PACKET pkt;
	int i, *p;
	ssize_t err;
	u_char echo_scan;

	/* We are already doing a radar scan, that's not good */
	if(radar_scan_mutex)
		return 1;
	radar_scan_mutex=1;	
	
	/* We create the PACKET */
	memset(&pkt, '\0', sizeof(PACKET));
	inet_setip_bcast(&pkt.to, my_family);
	pkt.sk_type=SKT_BCAST;
	my_echo_id=random();

	gettimeofday(&scan_start, 0);

	/* Send a bouquet of ECHO_ME pkts */
	if(me.cur_node->flags & MAP_HNODE) {
		pkt.hdr.sz=sizeof(u_char);
		pkt.msg=xmalloc(pkt.hdr.sz);
	}
	for(i=0, echo_scan=0; i<MAX_RADAR_SCANS; i++, echo_scan++) {
		if(me.cur_node->flags & MAP_HNODE)
			memcpy(pkt.msg, &echo_scan, sizeof(u_char));

		err=send_rq(&pkt, 0, ECHO_ME, my_echo_id, 0, 0, 0);
		if(err==-1) {
			error("radar_scan(): Error while sending the scan 0x%x"
					"... skipping", my_echo_id);
			continue;
		}
		radar_scans++;
	}
	
	if(!radar_scans) {
		error("radar_scan(): The scan 0x%x failed. It wasn't possible" 
				"to send a single scan", my_echo_id);
		return -1;
	}

	pkt_free(&pkt, 1);
	
	sleep(max_radar_wait);

	final_radar_queue();
	radar_update_map();

	if(!(me.cur_node->flags & MAP_HNODE)) {
		for(i=0; i<me.cur_quadg.levels; i++)
			if(send_qspn_now[i]) {
				p=xmalloc(sizeof(int));
				*p=i;
				/* We start a new qspn_round in the level `i' */
				pthread_create(&thread, &radar_qspn_send_t_attr, 
						radar_qspn_send_t, (void *)p);
			}
		reset_radar();
	}

	radar_scan_mutex=0;	
	return 0;
}


/* 
 * radard: It sends back to rpkt.from the ECHO_REPLY pkt in reply to the ECHO_ME
 * pkt received.
 */
int radard(PACKET rpkt)
{
	PACKET pkt;
	struct radar_queue *rq;
	ssize_t err;
	char *ntop; 
	u_char echo_scans_count;

	/* If we are hooking we reply only to others hooking nodes */
	if(me.cur_node->flags & MAP_HNODE) {
		if(rpkt.hdr.flags & HOOK_PKT) {
			memcpy(&echo_scans_count, rpkt.msg, sizeof(u_char));

			/* 
			 * So, we are hooking, but we haven't yet started the
			 * first scan or we have done less scans, 
			 * this means that this node, who is hooking
			 * too and sent us this rpkt, has started the hook 
			 * before us. If we are in a black zone, this flag
			 * will be used to decide which of the hooking nodes
			 * have to create the new gnode: if it is set we'll wait,
			 * the other hooking node will create the gnode, then we
			 * restart the hook. Clear?
			 */
			if(!radar_scan_mutex || echo_scans_count > radar_scans) {
				hook_retry=1;
				debug(DBG_NOISE, "Hooking node caught. s: %d, esc: %d," 
						"hook_retry: %d", radar_scans, 
						echo_scans_count, hook_retry);
			}
		} else {
			debug(DBG_NOISE, "ECHO_ME pkt dropped: We are hooking");	
			return 0;
		}
	}

	/* We create the ECHO_REPLY pkt */
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rpkt.from);
	pkt_addsk(&pkt, rpkt.from.family, rpkt.sk, SKT_UDP);
	if(me.cur_node->flags & MAP_HNODE) {
		/* 
		 * We attach in the ECHO_REPLY a flag that indicates if we have
		 * finished our radar_scan or not. This is usefull if we already
		 * sent all the ECHO_ME pkts of our radar scan and while we are
		 * waiting the MAX_RADAR_WAIT another node start the hooking:
		 * with this flag it can know if we came before him.
		 */
		u_char scanning=1;
		
		pkt.hdr.sz=sizeof(u_char);
		pkt.msg=xmalloc(pkt.hdr.sz);
		if(radar_scans==MAX_RADAR_SCANS)
			scanning=0;
		memcpy(pkt.msg, &scanning, sizeof(u_char));

		/* 
		 * W Poetry Palazzolo, the enlightening holy garden.
		 * Sat Mar 12 20:41:36 CET 2005 
		 */
	}

	/* We send it */
	err=send_rq(&pkt, 0, ECHO_REPLY, rpkt.hdr.id, 0, 0, 0);
	pkt_free(&pkt, 0);
	if(err==-1) {
		ntop=inet_to_str(pkt.to);
		error("radard(): Cannot send back the ECHO_REPLY to %s.", ntop);
		xfree(ntop);
		return -1;
	}

	/* 
	 * Ok, we have sent the reply, now we can update the radar_queue with
	 * calm.
	 */
	rq=add_radar_q(rpkt);
	rq->pings++;
	
	return 0;
}

/* Oh, what a simple daemon ^_^ */
void *radar_daemon(void *null)
{
	debug(DBG_NORMAL, "Radar daemon up & running");

	for(;;)
		radar_scan();
}


/* 
 * refresh_hook_root_node: At hooking the radar_scan doesn't have an int_map, so
 * all the nodes it found are stored in fake nodes. When we finish the hook,
 * instead, we have an int_map, so we convert all this fake nodes into real
 * nodes. To do this we modify each rq->node of the radar_queue and recall the
 * radar_update_map() func. 
 * Note: the me.cur_node must be deleted prior the call of this function.
 */
int refresh_hook_root_node(void)
{
	struct radar_queue *rq;
	map_node *rnode;
	int ret;

	rq=radar_q;
	list_for(rq) {
		ret=iptomap((int)me.int_map, rq->ip, me.cur_quadg.ipstart[1],
				(u_int *)&rnode);
		if(ret)
			rq->node=(map_node *)RADQ_EXT_RNODE;
		else
			rq->node=rnode;
	}

	radar_update_map();

	return 0;
}

/*EoW*/
