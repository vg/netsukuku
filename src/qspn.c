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
#include "tracer.h"
#include "qspn.h"
#include "netsukuku.h"
#include "request.h"
#include "log.h"
#include "xmalloc.h"
#include "misc.h"

extern int my_family;
extern struct current me;

void qspn_set_map_vars(u_char level, map_node **map, map_node **root_node, 
		int *root_node_pos, map_gnode **gmap)
{
	if(!level) {
		if(map)
			*map=me.int_map;
		if(root_node)
			*root_node=me.cur_node;
		if(root_node_pos)
			*root_node_pos=pos_from_node(me.cur_node, me.int_map);
	} else {
		if(map)
			*map=(map_node *)me.ext_map[_EL(level)];
		if(gmap)
			*gmap=me.ext_map[_EL(level)];
		if(root_node)
			*root_node=&me.cur_quadg.gnode[_EL(level)]->g;
		if(root_node_pos)
			*root_node_pos=pos_from_gnode(me.cur_quadg.gnode[_EL(level)], me.ext_map[_EL(level)]);
	}
}

void qspn_init(u_char levels)
{
	qspn_b=xmalloc(sizeof(struct qspn_buffer)*levels);
	qspn_send_mutex=xmalloc(sizeof(int)*levels);
	me.cur_qspn_id=xmalloc(sizeof(int)*levels);
	me.cur_qspn_time=xmalloc(sizeof(struct timeval)*levels);
}

void qspn_free(void)
{
	xfree(qspn_b);
	xfree(qspn_send_mutex);
	xfree(me.cur_qspn_id);
	xfree(me.cur_qspn_time);
}

void qspn_b_clean(u_char level)
{
	int i;
	struct qspn_buffer *qb=qspn_b[level];
	list_for(qb) {
		if(qb->replies) {
			xfree(qb->replier);
			xfree(qb->flags);
		}
		memset(qb, 0, sizeof(struct qspn_buffer));
	}
}

/* 
 * qspn_b_add: It adds a new element in the qspn_b 'qb' buffer and returns its
 * position. 
 */
int qspn_b_add(struct qspn_buffer *qb, u_short replier, u_short flags)
{
	qb->replies++;
	qb->replier=xrealloc(qb->replier, sizeof(u_short)*qb->replies);
	qb->flags=xrealloc(qb->flags, sizeof(u_short)*qb->replies);
	
	qb->replier[qb->replies-1]=replier;
	qb->flags[qb->replies-1]=flags;
	
	return qb->replies-1;
}

int qspn_b_find_reply(struct qspn_buffer *qb, int sub_id)
{
	int i;

	for(i=0; i<qb->replies; i++)
		if(qb->replier[i] == sub_id)
			return i;
	return -1;
}

/* 
 * qspn_round_left: It returns the milliseconds left before the QSPN_WAIT_ROUND
 * expires. If the returned value is <= 0 the QSPN_WAIT_ROUND is expired.
 */
int qspn_round_left(u_char level)
{
	struct timeval cur_t, t;
	
	gettimeofday(&cur_t, 0);
	timersub(&cur_t, &me.cur_qspn_time[level], &t);
	return QSPN_WAIT_ROUND_MS_LVL(level) - MILLISEC(t);
}


/* 
 * update_qspn_time: It updates me.cur_qspn_time;
 * Oh, sorry this code doesn't show consideration for the relativity time shit.
 * So you can't move at a velocity near the light's speed. I'm sorry.
 */
void update_qspn_time(u_char level)
{
	struct timeval cur_t, t;
	int ret;

	gettimeofday(&cur_t, 0);
	timersub(&cur_t, &me.cur_qspn_time[level], &t);
	ret=QSPN_WAIT_ROUND_MS_LVL(level) - MILLISEC(t);

	if(ret < 0 && abs(ret) > QSPN_WAIT_ROUND_MS_LVL(level)) {
		ret=ret-(QSPN_WAIT_ROUND_MS_LVL(level)*(ret/QSPN_WAIT_ROUND_MS_LVL(level)));
		t.tv_sec=ret/1000;
		t.tv_usec=(ret - (ret/1000)*1000)*1000;
		timersub(&cur_t, &t, &me.cur_qspn_time[level]);
	}
}

/* 
 * qspn_new_round: It prepares all the buffers for the new qspn_round and 
 * removes the QSPN_OLD nodes from the map.
 */
void qspn_new_round(u_char level)
{
	int bm, i;
	map_node *map, *root_node, *node;
	map_gnode *gmap;
	
	qspn_set_map_vars(level, &map, &root_node, 0, &gmap);

	/* New round activated. Destroy the old one. beep. */
	me.cur_qspn_id[level]++;
	update_qspn_time(level);
	qspn_b_clean(level);

	root_node->flags&=~QSPN_STARTER;
	for(i=0; i<root_node->links; i++) {
		node=(map_node *)root_node->r_node[i].r_node;
		node->flags&=~QSPN_CLOSED & ~QSPN_REPLIED;
	}

	/*
	 * How to remove the dead nodes from the map? How do we know which are deads?
	 * Pretty simple, we can't know so we wait until the next qspn_round to break them
	 * if they didn't show in the while. 
	 */
	for(i=0; i<MAXGROUPNODE; i++) {
		if((map[i].flags & QSPN_OLD) && !(map[i].flags & MAP_VOID)) {
			if((map[i].flags & MAP_BNODE)) {
				if((bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], map, &map[i], level))!=-1)
					me.bnode_map[level]=map_bnode_del(me.bnode_map[level], &me.bmap_nodes[level], 
							&me.bnode_map[level][bm]);
			}

			if(!level)
				map_node_del(&map[i]);
			else
				gmap_node_del(gmap);

			if(level != me.cur_quadg.levels-1)
				me.cur_quadg.gnode[level+1]->seeds--;
		} else
			map[i].flags|=QSPN_OLD;
	}
}

/* Exclude function. (see tracer.c) */
int exclude_from_and_opened_and_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	struct qspn_buffer *qb=qspn_b[excl_level], *qbp;
	int reply;

	qbp=list_pos(qb, pos);

	reply=qspn_b_find_reply(qb, sub_id);
	
	if(qbp->flags[reply] & QSPN_OPENED || 
			exclude_from_and_glevel(TRACER_PKT_EXCLUDE_VARS_NAME))
		return 1;
	return 0;
}

/*
 * The Holy qspn_send. It is used to send a new qspn_round when something 
 * changes around the root_node (me).
 */
int qspn_send(u_char level)
{
	PACKET pkt;
	map_node *from=me.cur_node;
	int round_ms, ret, upper_gid, root_node_pos;
	map_node *map, *root_node;
	map_gnode *gmap;
	u_char upper_level;
	
	qspn_set_map_vars(level, &map, &root_node, &root_node_pos, &gmap);
	upper_level=level+1;

	/* Now I explain how the level stuff in the qspn works. For example, if we want to
	 * propagate the qspn in the level 2, we store in qspn.level the upper level (3), and 
	 * the gid of the upper_level which containts the entire level 2. Simple no?
	 */

	
	/*If we aren't a bnode it's useless to send qspn in higher levels*/
	if(level && !(root_node->flags & MAP_BNODE))
		return -1;
	
	if(qspn_send_mutex[level])
		return 0;
	else
		qspn_send_mutex[level]=1;

	/*We have to wait the the finish of the old qspn_round to start the new one ^_-*/
	while((round_ms=qspn_round_left(level)) > 0) {
		usleep(round_ms);
		update_qspn_time(level);
	}
	
	qspn_new_round(level);
	root_node->flags|=QSPN_STARTER;

	/* The forge of the packet. "One pkt to rule them all". Dum dum */
	upper_gid=me.cur_quadg.gid[upper_level];
	tracer_pkt_build(QSPN_CLOSE, me.cur_qspn_id[level], root_node_pos, /*IDs*/
			 upper_gid,  level,
			 0,          0,         	    0, 		   /*Received tracer_pkt*/
			 0,          0,              	    0, 		   /*bnode_block*/
			 &pkt);						   /*Where the pkt is built*/
	/*... forward the qspn_opened to our r_nodes*/
	tracer_pkt_send(exclude_from_and_glevel_and_closed, upper_gid, 
			upper_level, -1, from, pkt);

	qspn_send_mutex[level]=0;
	return ret;
}

int qspn_close(PACKET rpkt)
{
	PACKET pkt;
	brdcast_hdr *bcast_hdr;
	tracer_hdr  *tracer_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	ssize_t err;
	size_t bblock_sz=0, old_bblock_sz;
	int i, not_closed=0, ret=0, new_qspn_close=0, ret_err;
	u_int hops;
	u_short old_bchunks=0;
	char *ntop, *old_bblock;

	map_node *from, *root_node, *tracer_starter, *node;
	map_gnode *gfrom, *gtracer_starter;
	void *void_map;
	int gid, root_node_pos;
	u_char level, upper_level;


	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &tracer_hdr, &tracer, &bhdr, &bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(&rpkt.from);
		debug(DBG_NOISE, "qspn_close(): The %s node sent an invalid "
				"qspn_close pkt here.", ntop);
		xfree(ntop);
		return -1;
	}
	gid=bcast_hdr->g_node;
	upper_level=level=bcast_hdr->level;

	if(!level || level==1) {
		level=0;
		qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
		from=node_from_pos(tracer[hops-1].node, me.int_map);
		tracer_starter=node_from_pos(tracer[0].node, me.int_map);
                void_map=me.int_map;
	} else {
		level--;
		qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
		gfrom=gnode_from_pos(tracer[hops-1].node, me.ext_map[_EL(level)]);
		from=&gfrom->g;
		gtracer_starter=gnode_from_pos(tracer[0].node, 
				me.ext_map[_EL(level)]);
		tracer_starter=&gtracer_starter->g;
                void_map=me.ext_map;
	}
	
	if(root_node->flags & QSPN_STARTER || tracer_starter == root_node) {
		ntop=inet_to_str(&rpkt.from);
		debug(DBG_NOISE, "qspn_close(): We received a qspn_close from "
				"%s, but we are the QSPN_STARTER.", ntop);
		xfree(ntop);
		return 0;
	}
	
	if(rpkt.hdr.id != me.cur_qspn_id[level]) {
		if(qspn_round_left(level) > 0 || rpkt.hdr.id != me.cur_qspn_id[level]+1) {
			ntop=inet_to_str(&rpkt.from);
			debug(DBG_NOISE, "qspn_close(): The %s sent a qspn_close"
					" with a wrong qspn_id", ntop);
			xfree(ntop);
			return -1;
		} else
			qspn_new_round(level);
	}

	/*Time to update our map*/
	hops=tracer_hdr->hops;
	tracer_store_pkt(void_map, level, tracer_hdr, tracer, (void *)bhdr, bblock_sz,
			&old_bchunks, &old_bblock, &old_bblock_sz);

	/*
	 * Only if we are in the level 0, or if we are a bnode, we can do the real 
	 * qspn actions, otherwise we simply forward the pkt
	 */
	not_closed=0;
	if(!level || (root_node->flags & MAP_BNODE)) {
		/*
		 * We close the from node and we see there are any links still 
		 * `not_closed'.
		 */
		for(i=0; i<root_node->links; i++) {
			if(root_node->r_node[i].r_node == (u_int *)from) {
				node=(map_node *)root_node->r_node[i].r_node;
				node->flags|=QSPN_CLOSED;
			}

			if(!(node->flags & QSPN_CLOSED))
				not_closed++;
		}
	}

	/*We build d4 p4ck37...*/
	tracer_pkt_build(QSPN_CLOSE, rpkt.hdr.id, root_node_pos,  /*IDs*/
			 gid,	     level,
			 bcast_hdr, tracer_hdr, tracer, 	  /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz,  /*bnode_block*/
			 &pkt);					  /*Where the pkt is built*/
	xfree(old_bblock);

	/*We have all the links closed, time to diffuse a new qspn_open*/
	if(!not_closed && !(node->flags & QSPN_REPLIED)) {
		pkt.hdr.op=QSPN_OPEN;
		tracer_pkt_send(exclude_from_and_glevel_and_setreplied, gid, 
				upper_level, -1, from, pkt);
	} else {
		/*Forward the qspn_close to all our r_nodes!*/
		tracer_pkt_send(exclude_from_and_glevel_and_closed, gid, 
				upper_level, -1, from, pkt);
	}
finish:
	if(new_qspn_close) {
		xfree(bcast_hdr);
		xfree(tracer_hdr);
	}
	return ret;
}

int qspn_open(PACKET rpkt)
{
	PACKET pkt;
	brdcast_hdr  *bcast_hdr;
	tracer_hdr   *tracer_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	struct qspn_buffer *qb=0;
	ssize_t err;
	int i, not_opened=0, ret=0, reply, sub_id, ret_err;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	char *ntop, *old_bblock;

	map_node *from, *root_node;
	map_gnode *gfrom;
	void *void_map;
	int gid, root_node_pos;
	u_char level, upper_level;

	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &tracer_hdr, &tracer, &bhdr, &bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(&rpkt.from);
		debug(DBG_NOISE, "qspn_open(): The %s sent an invalid qspn_open pkt here.", ntop);
		xfree(ntop);
		return -1;
	}
	
	hops=tracer_hdr->hops;
	gid=bcast_hdr->g_node;
	upper_level=level=bcast_hdr->level;
	if(!level || level==1) {
		level=0;
		qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
		from=node_from_pos(tracer[hops-1].node, me.int_map);
                void_map=me.int_map;
	} else {
		level--;
		qspn_set_map_vars(level, 0, &root_node, &root_node_pos, 0);
		gfrom=gnode_from_pos(tracer[hops-1].node, me.ext_map[_EL(level)]);
		from=&gfrom->g;
                void_map=me.ext_map;
	}

	sub_id=bcast_hdr->sub_id;
	if(sub_id == root_node_pos) {
		ntop=inet_to_str(&rpkt.from);
		debug(DBG_NOISE, "qspn_open(): We received a qspn_open from %s, but we are the opener.", ntop);
		xfree(ntop);
		return 0;
	}

	if(rpkt.hdr.id != me.cur_qspn_id[level]) {
		ntop=inet_to_str(&rpkt.from);
		debug(DBG_NOISE, "qspn_open(): The %s sent a qspn_open with a wrong qspn_id", ntop);
		xfree(ntop);
		return -1;
	}

	/*Time to update our map*/
	tracer_store_pkt(void_map, level, tracer_hdr, tracer, (void *)bhdr, bblock_sz,
			&old_bchunks, &old_bblock, &old_bblock_sz);
	
	/* 
	 * We search in the qspn_buffer the reply which has current sub_id. 
	 * If we don't find it, we add it.
	 */
	if((reply=qspn_b_find_reply(qspn_b[level], sub_id))==-1) {
		qb=qspn_b[level];
		list_for(qb)
			if(qb->rnode == from)
				reply=qspn_b_add(qb, sub_id, 0);
	}

	/*
	 * Only if we are in the level 0, or if we are a bnode, we can do the real 
	 * qspn actions, otherwise we simply forward the pkt
	 */
	not_opened=0;
	if(!level || (root_node->flags & MAP_BNODE)) {
		/*Time to open the links*/
		qb=qspn_b[level];
		list_for(qb) {
			if(qb->rnode == from)
				qb->flags[reply]|=QSPN_OPENED;
			if(!(qb->flags[reply] & QSPN_OPENED))
				not_opened++;
		}
		/*Fokke, we've all the links opened. let's take a rest.*/
		if(!not_opened) {
			debug(DBG_NOISE, "qspn_open(): We've finished the qspn_open (sub_id: %d) phase", sub_id);
			return 0;
		}
	}

	/* The forge of the packet. "One pkt to rule them all". Dum dum */
	tracer_pkt_build(QSPN_OPEN, rpkt.hdr.id, bcast_hdr->sub_id,  			   /*IDs*/
			 gid, 	    level,
			 bcast_hdr, tracer_hdr, tracer, 				   /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz, 			   /*bnode_block*/
			 &pkt);								   /*Where the pkt is built*/
	xfree(old_bblock);
	/*...forward the qspn_opened to our r_nodes*/
	tracer_pkt_send(exclude_from_and_opened_and_glevel, gid, upper_level, 
			sub_id, from, pkt);
	return ret;
}
