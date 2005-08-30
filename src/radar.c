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
 *
 * --
 *  
 * radar.c:
 * The radar sends in broadcast a bouquet of MAX_RADAR_SCANS# packets and waits
 * for the ECHO_REPLY of the nodes which are alive. It then recollects the
 * replies and builds a small statistic, updates, if necessary, the internal 
 * maps, the bnode maps and the qspn buffer.
 * A radar is fired periodically by the radar_daemon(), which is started as a
 * thread.
 */

#include "includes.h"

#include "llist.c"
#include "inet.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "route.h"
#include "request.h"
#include "pkts.h"
#include "qspn.h"
#include "radar.h"
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
	max_radar_wait=MAX_RADAR_WAIT;	
	
	pthread_attr_init(&radar_qspn_send_t_attr);
	pthread_attr_setdetachstate(&radar_qspn_send_t_attr, PTHREAD_CREATE_DETACHED);	 
	
	list_init(radar_q, 0);
	radar_q_counter=0;
	
	/* register the radar's ops in the pkt_op_table */
	add_pkt_op(ECHO_ME, SKT_BCAST, ntk_udp_radar_port, radard);
	add_pkt_op(ECHO_REPLY, SKT_UDP, ntk_udp_radar_port, radar_recv_reply);
	
	memset(send_qspn_now, 0, sizeof(u_char)*MAX_LEVELS);
}


void close_radar(void)
{
	struct radar_queue *rq;
	rq=radar_q;

	if(radar_q_counter)
		list_destroy(radar_q);
	radar_q_counter=0;
	radar_q=0;

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

struct radar_queue *find_node_radar_q(map_node *node)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq)
		if(rq->node==node)
			return rq;
	return 0;
}

struct radar_queue *find_ip_radar_q(inet_prefix *ip)
{
	struct radar_queue *rq;

	rq=radar_q;
	list_for(rq)
		if(!memcmp(&rq->ip, ip, sizeof(inet_prefix)))
			return rq;
		
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

		for(e=0; e < rq->pongs; e++)
			timeradd(&rq->rtt[e], &sum, &sum);
		
		/* Add penality rtt for each pong lost */
		for(; e < MAX_RADAR_SCANS; e++)
			timeradd(&rq->rtt[e-rq->pongs], &sum, &sum);

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
	map_node *node, *root_node, *broot_node;
	map_gnode *gnode;
	map_bnode *bnode;
	ext_rnode *e_rnode;
	ext_rnode_cache *erc;
	struct qspn_buffer *qb;
	int i, e, node_pos, bm, rnode_pos, bnode_rnode_pos, root_node_pos;
	int broot_node_pos;
	int level, blevel, external_node, total_levels, first_level;
	void *void_map, *void_gnode;

	if(!me.cur_node->links)
		return 0;

	for(i=0; i<me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;

		if(!(node->flags & MAP_VOID))
			/* The rnode is not really dead! */
			continue;

		if(node->flags & MAP_ERNODE) {
			e_rnode=(ext_rnode *)node;
			external_node=1;
			total_levels=e_rnode->quadg.levels;
			first_level=1;
		} else {
			external_node=0;
			total_levels=1;
			first_level=0;
		}

		for(level=first_level; level < total_levels; level++) {
			qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
			blevel=level-1;

			/*
			 * Just delete it from all the maps.
			 * We don't care to send the qspn to inform the other nodes of this death. 
			 * They will wait till the next qspn round to know it.
			 */
			
			if(!level && !external_node) {
				void_map=me.int_map;
				node_pos=pos_from_node(node, me.int_map);
				rnode_pos=i;
				
				debug(DBG_NORMAL, "radar: The node %d is dead", 
						node_pos);
				map_node_del(node);
			} else {
				void_map=me.ext_map;
				gnode=e_rnode->quadg.gnode[_EL(level)];
				void_gnode=(void *)gnode;
				if(!void_gnode)
					continue;
				node_pos=pos_from_gnode(gnode, me.ext_map[_EL(level)]); 
				rnode_pos=g_rnode_find((map_gnode *)root_node, gnode);

				debug(DBG_NORMAL, "The ext_node (gid %d, lvl %d) is"
						" dead", e_rnode->quadg.gid[level], level);
				gmap_node_del(gnode);

				/* bnode_map update */
				for(e=0; blevel >= 0; blevel--) {
					qspn_set_map_vars(blevel, 0, &broot_node, &broot_node_pos, 0);
					bm=map_find_bnode(me.bnode_map[blevel], me.bmap_nodes[blevel],
							broot_node_pos);
					if(bm == -1)
						continue;

					bnode=&me.bnode_map[blevel][bm];
					bnode_rnode_pos=rnode_find(bnode, 
							(map_node *) e_rnode->quadg.gnode[_EL(level)]);
					if(bnode_rnode_pos != -1)
						rnode_del(bnode, bnode_rnode_pos);

					if(!bnode->links) {
						me.bnode_map[blevel]=map_bnode_del(me.bnode_map[blevel], 
								&me.bmap_nodes[blevel], bnode);
						broot_node->flags&=~MAP_BNODE;
					} else
						e=1;
				}
				if(!e)
					me.cur_node->flags&=~MAP_BNODE;

				/* Delete the entries from the routing table */
				if(level == 1)
				  krnl_update_node(&e_rnode->quadg.ipstart[0], 
						  e_rnode, 0, me.cur_node, 0);
				krnl_update_node(0, 0, &e_rnode->quadg, 0, level);
			}
		
			rnode_del(root_node, rnode_pos);
			if(!root_node->links) {
				/* We are alone in the dark. Sigh. */
				qspn_time_reset(level, level, GET_LEVELS(my_family));
			} else if(!external_node)
				erc_update_rnodepos(me.cur_erc, root_node, rnode_pos);

			/* Now we delete it from the qspn_buffer */
			if(qspn_b[level]) {
				qb=qspn_b[level];
				qb=qspn_b_find_rnode(qb, node);
				if(qb)
					qspn_b[level]=list_del(qspn_b[level], qb);
			}

			rnode_deleted[level]++;
		}
		
		/* 
		 * Kick out the external_node from the root_node and destroy it
		 * from the ext_rnode_cache
		 */
		if(external_node) {
			/* external rnode cache update */
			erc=erc_find(me.cur_erc, e_rnode);
			if(erc)
				e_rnode_del(&me.cur_erc, &me.cur_erc_counter, erc);
			rnode_del(me.cur_node, i);
		}

		/* If the rnode we deleted from the root_node was swapped with
		 * the last rnodes, we have to inspect again the same
		 * root_node->r_node[ `i' ] rnode, because now it is another 
		 * rnode */
		if(i != (me.cur_node->links+1) - 1)
			i--;
	}
	return 0;
}

/* 
 * radar_update_bmap: updates the bnode map of the given `level': the root_node
 * bnode in the bmap will also point to the gnode of level `gnode_level'+1 that
 * is `rq'->quadg.gnode[_EL(gnode_level+1)].
 */
void radar_update_bmap(struct radar_queue *rq, int level, int gnode_level)
{
	map_gnode *gnode;
	map_node  *root_node;
	map_rnode *rnode, rn;
	int  bm, rnode_pos, root_node_pos;
	void *void_map;

	if(level == me.cur_quadg.levels-1)
		return;

	qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
	void_map=me.ext_map;
	gnode=rq->quadg.gnode[_EL(gnode_level+1)];
	
	bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level],
			root_node_pos);
	if(bm==-1) {
		bm=map_add_bnode(&me.bnode_map[level], &me.bmap_nodes[level], 
				root_node_pos, 0);
		rnode_pos=-1;
	} else
		rnode_pos=rnode_find(&me.bnode_map[level][bm], &gnode->g);
	
	if(rnode_pos == -1) {
		memset(&rn, 0, sizeof(map_rnode));
		rn.r_node=(int *)&gnode->g;
		rnode_add(&me.bnode_map[level][bm], &rn);
		rnode_pos=0;
	}

	rnode=&me.bnode_map[level][bm].r_node[rnode_pos];
	memcpy(&rnode->rtt, &rq->final_rtt, sizeof(struct timeval));
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
	struct qspn_buffer *qb;
	struct radar_queue *rq;
	ext_rnode_cache *erc;
	map_gnode *gnode;
	map_node  *node, *root_node;
	map_rnode rnn, *new_root_rnode;
	ext_rnode *e_rnode;
	int i, e, diff, updated_rnodes;
	int rnode_added[MAX_LEVELS], rnode_deleted[MAX_LEVELS], rnode_pos;
	int level, external_node, total_levels, root_node_pos, node_update;
	void *void_map;
	const char *ntop;

	updated_rnodes=0;
	memset(rnode_added, 0, sizeof(int)*MAX_LEVELS);
	memset(rnode_deleted, 0, sizeof(int)*MAX_LEVELS);
	
	/*
	 * Let's consider all our rnodes void, in this way we'll know what
	 * rnodes will remain void after the update.
	 */
	for(i=0; i<me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;
		node->flags|=MAP_VOID | MAP_UPDATE;
		
		if(node->flags & MAP_GNODE || node->flags & MAP_ERNODE) {
			e_rnode=(ext_rnode *)node;
			
			for(e=1; e<e_rnode->quadg.levels; e++) {
				gnode=e_rnode->quadg.gnode[_EL(e)];
				if(!gnode)
					continue;
				gnode->g.flags|=MAP_VOID | MAP_UPDATE;
			}
		}
	}

	rq=radar_q;
	list_for(rq) {
	           if(!rq->node)
			   continue;
		   if(!(me.cur_node->flags & MAP_HNODE) && (rq->flags & MAP_HNODE))
			   continue;

		   /* 
		    * We need to know if it is a node which is not in the gnode
		    * where we are (external_rnode).
		    */
		   if((int)rq->node == RADQ_EXT_RNODE) {
			   external_node=1;
			   total_levels=rq->quadg.levels;
		   } else {
			   external_node=0;
			   total_levels=1;
		   }

		   for(level=total_levels-1; level >= 0; level--) {
			   qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
			   node_update=0;

			   if(!level) {
				   void_map=me.int_map;
				   node=rq->node;
			   } else {
				   /* Skip the levels where the ext_rnode belongs
				    * to our same gids */
				   if(!quadg_gids_cmp(rq->quadg, me.cur_quadg, level))
					   continue;
				   
				   /* Update only the gnodes which belongs to
				    * our same gid of the upper level, because
				    * we don't keep the internal info of the
				    * extern gnodes. */
				   if((level < rq->quadg.levels-1) &&
					quadg_gids_cmp(rq->quadg, me.cur_quadg, level+1)) {
					   rq->quadg.gnode[_EL(level)]=0;
					   continue;
				   }
				   
				   /* Ehi, we are a bnode */
				   root_node->flags|=MAP_BNODE;
				   me.cur_node->flags|=MAP_BNODE;
				   
				   void_map=me.ext_map;
				   gnode=rq->quadg.gnode[_EL(level)];
				   node=&gnode->g;
			   }

			   if(external_node && !level && me.cur_erc_counter) {
				   erc=e_rnode_find(me.cur_erc, &rq->quadg, 0);
				   if(!erc)
					   rnode_pos=-1;
				   else {
					   rnode_pos=erc->rnode_pos;
					   node=(map_node *)erc->e;
				   }
			   } else
				   rnode_pos=rnode_find(root_node, node);

			   if(rnode_pos == -1) { /* W00t, we've found a new rnode! */
				   node_update=1;
				   rnode_pos=root_node->links; 
				   
				   ntop=inet_to_str(rq->quadg.ipstart[level]);
				   loginfo("Radar: New node found: %s, ext: %d, level: %d", 
						   ntop, external_node, level);

				   if(external_node && !level) {
					   /* 
					    * If this node we are processing is external, at level 0,
					    * in the root_node's rnodes we add a rnode which point 
					    * to a ext_rnode struct.
					    */

					   memset(&rnn, '\0', sizeof(map_rnode));
					   e_rnode=xmalloc(sizeof(ext_rnode));
					   memset(e_rnode, 0, sizeof(ext_rnode));

					   memcpy(&e_rnode->quadg, &rq->quadg, sizeof(quadro_group));
					   e_rnode->node.flags=MAP_BNODE | MAP_GNODE |  MAP_RNODE | 
						   MAP_ERNODE;
					   rnn.r_node=(int *)e_rnode;
					   node=rq->node=&e_rnode->node;
					   new_root_rnode=&rnn;
					  
					   /* Update the external_rnode_cache list */
					   e_rnode_add(&me.cur_erc, e_rnode, rnode_pos,
							   &me.cur_erc_counter);
				   } else {
					   /*We purge all the node's rnodes.*/
					   rnode_destroy(node);

					   /* 
					    * This node has only one rnode, 
					    * and that is the root_node.
					    */
					   memset(&rnn, '\0', sizeof(map_rnode));
					   rnn.r_node=(int *)root_node;
					   rnode_add(node, &rnn);

					   /* It is a border node */
					   if(level)
						   node->flags|=MAP_BNODE | MAP_GNODE;
					   node->flags|=MAP_RNODE;

					   /* 
					    * Fill the rnode to be added in the
					    * root_node.
					    */
					   memset(&rnn, '\0', sizeof(map_rnode));
					   rnn.r_node=(int *)node; 
					   new_root_rnode=&rnn;
				   }

				   /* 
				    * The new node is added in the root_node's
				    * rnodes.
				    */
				   rnode_add(root_node, new_root_rnode);

				   /* Update the qspn_buffer */
				   if(!external_node || level) {
					   qb=xmalloc(sizeof(struct qspn_buffer));
					   memset(qb, 0, sizeof(struct qspn_buffer));
					   qb->rnode=node;
					   if(root_node->links == 1 || !qspn_b[level])
						   list_init(qspn_b[level], qb);
					   else
						   list_add(qspn_b[level], qb);

					   send_qspn_now[level]=1;
				   }
				   
				   rnode_added[level]++;
			   } else {
				   /* 
				    * Nah, We have the node in the map. Let's if its rtt is changed
				    */
				   if(!send_qspn_now[level] && node->links) {
					   diff=abs(MILLISEC(root_node->r_node[rnode_pos].rtt) -
							   MILLISEC(rq->final_rtt));
					   if(diff >= RTT_DELTA) {
				   		   node_update=1;
						   send_qspn_now[level]=1;
						   debug(DBG_NOISE, "node %s rtt changed, diff: %d",
								   inet_to_str(rq->ip), diff);
					   }
				   }
			   }
			   
			   /* Restore the flags */
			   if(level)
				   gnode->flags&=~GMAP_VOID;
			   node->flags&=~MAP_VOID & ~MAP_UPDATE & ~QSPN_OLD;

			   /* Nothing is really changed */
			   if(!node_update)
				   continue;
			   
			   /* Update the rtt */
		           memcpy(&root_node->r_node[rnode_pos].rtt, &rq->final_rtt,
					   sizeof(struct timeval));
			   
			   /* Bnode map stuff */
			   if(external_node && level) {
				   /* 
				    * All the root_node bnode which are in the
				    * bmaps of level less than `level' points to
				    * the same gnode which is rq->quadg.gnode[_EL(level-1+1)].
				    * This is because the inferior levels cannot
				    * have knowledge about the bordering gnode 
				    * which is in an upper level, but it's necessary that
				    * they know who the root_node borderes on,
				    * so the get route algorithm can descend to
				    * the inferior levels and it will still know
				    * what is the border node which is linked
				    * to the target gnode.
				    */
				   for(i=0; i < level; i++)
					   radar_update_bmap(rq, i, level-1);
				   send_qspn_now[level-1]=1;
			   }

			   if(external_node && node_update)
				   node->flags|=MAP_UPDATE;

		   } /*for(level=0, ...)*/
		   
		   updated_rnodes++;
	} /*list_for(rq)*/

	/* Burn the deads */
	if(updated_rnodes < me.cur_node->links)
		radar_remove_old_rnodes(rnode_deleted);

	/* <<keep your room tidy... order, ORDER>> */
	if(is_bufzero((char *)rnode_added, sizeof(int)*MAX_LEVELS) || 
			is_bufzero((char *)rnode_deleted, sizeof(int)*MAX_LEVELS)) {
		rnode_rtt_order(me.cur_node);
		for(i=1; i<me.cur_quadg.levels; i++)
			if(rnode_added[i] || rnode_deleted[i])
				rnode_rtt_order(&me.cur_quadg.gnode[_EL(i)]->g);
	}

	/* Give a refresh to the kernel */
	if(is_bufzero((char *)rnode_added, sizeof(int)*MAX_LEVELS))
		rt_rnodes_update(1);
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
		 * a stable ip, so we create fake nodes that will be delete after
		 * the hook.
		 */
		if(!(rq=find_ip_radar_q(&pkt.from))) {
			map_rnode rnn;

			rnode=xmalloc(sizeof(map_node));
			memset(rnode, '\0', sizeof(map_node));
			memset(&rnn, '\0', sizeof(map_rnode));

			rnn.r_node=(int *)me.cur_node;
			rnode_add(rnode, &rnn);
		} else
			rnode=rq->node;
	} 
	
	iptoquadg(pkt.from, me.ext_map, &quadg, QUADG_GID|QUADG_GNODE|QUADG_IPSTART);

	if(!(me.cur_node->flags & MAP_HNODE)) {
		iptomap((u_int)me.int_map, pkt.from, me.cur_quadg.ipstart[1],
				(u_int *)&rnode);
		ret=quadg_gids_cmp(me.cur_quadg, quadg, 1);
	}

	if(!ret)
		rq=find_node_radar_q(rnode);
	else
		rq=find_ip_radar_q(&pkt.from);
	
	/* If pkt.from isn't already in the queue, add it. */
	if(!rq) { 
		rq=xmalloc(sizeof(struct radar_queue));
		memset(rq, 0, sizeof(struct radar_queue));
		
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
		
		list_add(radar_q, rq);
		radar_q_counter++;
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
	if(!my_echo_id || !radar_scan_mutex || !radar_scans || !radar_q)
		return -1;
	
	if(pkt.hdr.id != my_echo_id) {
		debug(DBG_NORMAL,"I received an ECHO_REPLY with id: 0x%x, but "
				"my current ECHO_ME is 0x%x", pkt.hdr.id, 
				my_echo_id);
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
 * round if something is changed in the map and if the `activate_qspn' argument
 * is non zero.
 * It returns 1 if another radar_scan is in progress, -1 if something went
 * wrong, 0 on success.
 */
int radar_scan(int activate_qspn) 
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
	
	/*
	 * We create the PACKET 
	 */
	memset(&pkt, '\0', sizeof(PACKET));
	inet_setip_bcast(&pkt.to, my_family);
	my_echo_id=rand();

	gettimeofday(&scan_start, 0);

	
	/*
	 * Send a bouquet of ECHO_ME pkts 
	 */
	
	if(me.cur_node->flags & MAP_HNODE) {
		pkt.hdr.sz=sizeof(u_char);
		pkt.hdr.flags|=HOOK_PKT;
		pkt.msg=xmalloc(pkt.hdr.sz);
		debug(DBG_INSANE, "Radar scan 0x%x activated", my_echo_id);
	} else
		total_radars++;

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

	if(activate_qspn)
		for(i=0; i<me.cur_quadg.levels; i++)
			if(send_qspn_now[i]) {
				p=xmalloc(sizeof(int));
				*p=i;
				/* We start a new qspn_round in the level `i' */
				pthread_create(&thread, &radar_qspn_send_t_attr, 
						radar_qspn_send_t, (void *)p);
			}

	if(!(me.cur_node->flags & MAP_HNODE))
			reset_radar();

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
	const char *ntop; 
	u_char echo_scans_count;

	/* If we are hooking we reply only to others hooking nodes */
	if(me.cur_node->flags & MAP_HNODE) {
		if(rpkt.hdr.flags & HOOK_PKT) {
			memcpy(&echo_scans_count, rpkt.msg, sizeof(u_char));

			/* 
			 * So, we are hooking, but we haven't yet started the
			 * first scan or we have done less scans than rpkt.from,
			 * this means that this node, who is hooking
			 * too and sent us this rpkt, has started the hook 
			 * before us. If we are in a black zone, this flag
			 * will be used to decide which of the hooking nodes
			 * have to create the new gnode: if it is set we'll wait,
			 * the other hooking node will create the gnode, then we
			 * restart the hook. Clear?
			 */
			if(!radar_scan_mutex || echo_scans_count >= radar_scans)
				hook_retry=1;
		} else {
			/*debug(DBG_NOISE, "ECHO_ME pkt dropped: We are hooking");*/
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
		pkt.hdr.flags|=HOOK_PKT;
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
		error("radard(): Cannot send back the ECHO_REPLY to %s.", ntop);
		return -1;
	}

	/* 
	 * Ok, we have sent the reply, now we can update the radar_queue with
	 * calm.
	 */
	if(radar_q) {
		rq=add_radar_q(rpkt);
		rq->pings++;

#ifdef DEBUG
		if(server_opt.dbg_lvl && rq->pings==1 &&
				me.cur_node->flags & MAP_HNODE) {
			ntop=inet_to_str(pkt.to);
			debug(DBG_INSANE, "%s(0x%x) to %s", rq_to_str(ECHO_REPLY), 
					rpkt.hdr.id, ntop);
		}
#endif
	}
	return 0;
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
		ret=iptomap((u_int)me.int_map, rq->ip, me.cur_quadg.ipstart[1],
				(u_int *)&rnode);
		if(ret)
			rq->node=(map_node *)RADQ_EXT_RNODE;
		else
			rq->node=rnode;
	}

	radar_update_map();

	return 0;
}

/* Oh, what a simple daemon ^_^ */
void *radar_daemon(void *null)
{
	debug(DBG_NORMAL, "Radar daemon up & running");
	for(;;radar_scan(1));
}

/* radar_wait_new_scan: It sleeps until the new radar scan is sent */
void radar_wait_new_scan(void)
{
	int old_echo_id=my_echo_id;
	for(; old_echo_id == my_echo_id; )
		usleep(500000);
}


/*EoW*/
