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
			*root_node_pos=pos_from_gnode(me.cur_quadg.gnode[_EL(level)],
					me.ext_map[_EL(level)]);
	}
}

/* 
 * qspn_time_reset: Reset the qspn time of all the levels that go from
 * `start_level' to `end_level'. The total number of effective levels is
 * specified in `levels'.
 */
void qspn_time_reset(int start_level, int end_level, int levels)
{
	struct timeval cur_t;
	int i;

	if(end_level <= start_level)
		end_level = start_level+1;

	/* 
	 * We fake the cur_qspn_time, so qspn_round_left thinks that a
	 * qspn_round was already sent 
	 */
	gettimeofday(&cur_t, 0);
	cur_t.tv_sec-=QSPN_WAIT_ROUND_LVL(levels)*2;
	
	for(i=start_level; i < end_level; i++)
		memcpy(&me.cur_qspn_time[i], &cur_t, sizeof(struct timeval));
}

void qspn_init(u_char levels)
{
	qspn_b=xmalloc(sizeof(struct qspn_buffer *)*levels);
	memset(qspn_b, 0, sizeof(struct qspn_buffer *)*levels);
	
	qspn_send_mutex=xmalloc(sizeof(int)*levels);
	memset(qspn_send_mutex, 0, sizeof(int)*levels);
	
	me.cur_qspn_id=xmalloc(sizeof(int)*levels);
	memset(me.cur_qspn_id, 0, sizeof(int)*levels);
	
	me.cur_qspn_time=xmalloc(sizeof(struct timeval)*levels);

	qspn_time_reset(0, levels, levels);
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
	struct qspn_buffer *qb=qspn_b[level];
	list_for(qb) {
		if(qb->replies) {
			if(qb->replier)
				xfree(qb->replier);
			if(qb->flags)
				xfree(qb->flags);
			qb->replies=0;
			qb->replier=0;
			qb->flags=0;
		}
	}
}

/* 
 * qspn_b_add: It adds a new element in the qspn_b 'qb' buffer and returns its
 * position. 
 */
int qspn_b_add(struct qspn_buffer *qb, u_char replier, u_short flags)
{
	qb->replies++;
	qb->replier=xrealloc(qb->replier, sizeof(u_char)*qb->replies);
	qb->flags=xrealloc(qb->flags, sizeof(u_char)*qb->replies);
	
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
 * expires. If the round is expired it returns 0.
 */
int qspn_round_left(u_char level)
{
	struct timeval cur_t, t;
	int wait_round, cur_elapsed, diff;
	
	gettimeofday(&cur_t, 0);

	timersub(&cur_t, &me.cur_qspn_time[level], &t);

	wait_round  = QSPN_WAIT_ROUND_MS_LVL(level);
	cur_elapsed = MILLISEC(t);
	diff = wait_round - cur_elapsed;

	return cur_elapsed >= wait_round ? 0 : diff;
}


/* 
 * update_qspn_time: It updates me.cur_qspn_time;
 * Oh, sorry this code doesn't show consideration for the relativity time shit.
 * So you can't move at a velocity near the light's speed. I'm sorry.
 */
void update_qspn_time(u_char level, struct timeval *new_qspn_time)
{
	struct timeval cur_t, t;
	int ret;

	gettimeofday(&cur_t, 0);

	if(new_qspn_time) {
		timersub(&cur_t, new_qspn_time, &me.cur_qspn_time[level]);
		memcpy(&t, new_qspn_time, sizeof(struct timeval));
	} else
		timersub(&cur_t, &me.cur_qspn_time[level], &t);

	ret=QSPN_WAIT_ROUND_MS_LVL(level) - MILLISEC(t);

	if(ret < 0 && abs(ret) > QSPN_WAIT_ROUND_MS_LVL(level)) {
		ret*=-1;
		/* 
		 * we round `ret' to take off the time of the passed round, then
		 * we can store in `ret' the number of ms passed since the
		 * latest round.
		 */
		ret=ret-(QSPN_WAIT_ROUND_MS_LVL(level)*(ret/QSPN_WAIT_ROUND_MS_LVL(level)));
		t.tv_sec=ret/1000;
		t.tv_usec=(ret - (ret/1000)*1000)*1000;
		
		/* 
		 * Now we can calculate when the last round has started, the
		 * result is stored in `me.cur_qspn_time[level]'
		 */
		timersub(&cur_t, &t, &me.cur_qspn_time[level]);
	}
}

/* 
 * qspn_new_round: It prepares all the buffers for the new qspn_round and 
 * removes the QSPN_OLD nodes from the map. The new qspn_round id is set 
 * to `new_qspn_id'. If `new_qspn_id' is zero then the id is incremented by one.
 * If `new_qspn_time' is not null, the qspn_time[level] is set to the current
 * time minus `new_qspn_time'.
 */
void qspn_new_round(u_char level, int new_qspn_id, struct timeval *new_qspn_time)
{
	int bm, i, node_pos;
	map_node *map, *root_node, *node;
	map_gnode *gmap;
	void *void_map;
	
	qspn_set_map_vars(level, 0, &root_node, 0, &gmap);
	map=me.int_map;
	if(!level)
		void_map=map;
	else 
		void_map=me.ext_map;
	

	/* New round activated. Destroy the old one. beep. */
	if(new_qspn_id)
		me.cur_qspn_id[level]=new_qspn_id;
	else
		me.cur_qspn_id[level]++;

	if(new_qspn_time)
		update_qspn_time(level, new_qspn_time);
	else
		update_qspn_time(level, 0);
	qspn_b_clean(level);

	/* Clear the flags set during the previous qspn */
	root_node->flags&=~QSPN_STARTER;
	for(i=0; i<root_node->links; i++) {
		node=(map_node *)root_node->r_node[i].r_node;
		node->flags&=~QSPN_CLOSED & ~QSPN_REPLIED & ~QSPN_STARTER;
	}

	/*
	 * How to remove the dead nodes from the map? How do we know which are 
	 * deads?
	 * Pretty simple, we can't know so we wait until the next qspn_round to
	 * break them if they didn't show in the while. 
	 */
	for(i=0; i<MAXGROUPNODE; i++) {
		node_pos=i;
		if(!level)
			node=(map_node *)&map[node_pos];
		else {
			node=(map_node *)&gmap[node_pos];
			if(gmap[node_pos].flags & GMAP_VOID)
				continue;
		}
			
		if(node->flags & MAP_ME || node->flags & MAP_VOID)
			continue;

		if((node->flags & QSPN_OLD)) {
			
			if((node->flags & MAP_BNODE)) {
				/* 
				 * The node is a boarder node, delete it from
				 * the bmap.
				 */
				bm=map_find_bnode(me.bnode_map[level],
						me.bmap_nodes[level], void_map, node_pos);
				if(bm != -1)
					me.bnode_map[level]=map_bnode_del(me.bnode_map[level], 
							&me.bmap_nodes[level],
							&me.bnode_map[level][bm]);
			}

			if(!level) {
				debug(DBG_NORMAL, "qspn: The node %d is dead", 
						i);
				map_node_del(node);
			} else {
				debug(DBG_NORMAL, "The groupnode %d of level %d"
						" is dead", i, level);
				gmap_node_del((map_gnode *)node);
			}

			me.cur_quadg.gnode[level]->seeds--;
		} else
			node->flags|=QSPN_OLD;
	}
}

/* Exclude functions. (see tracer.c) */
int exclude_from_and_opened_and_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	struct qspn_buffer *qb, *qbp;
	int reply;

	qb=qspn_b[excl_level-1];
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
	map_node *from;
	int round_ms, ret, upper_gid, root_node_pos, qid;
	map_node *map, *root_node;
	map_gnode *gmap;
	u_char upper_level;

	qid=me.cur_qspn_id[level];
	from=me.cur_node;
	upper_level=level+1;
	qspn_set_map_vars(level, &map, &root_node, &root_node_pos, &gmap);

	/* 
	 * Now I explain how the level stuff in the qspn works. For example, if 
	 * we want to propagate the qspn in the level 2, we store in qspn.level
	 * the upper level (3), and the gid of the upper_level which containts 
	 * the entire level 2. Simple no?
	 */

	
	/*If we aren't a bnode it's useless to send qspn in higher levels*/
	if(level && !(root_node->flags & MAP_BNODE))
		return -1;
	
	if(qspn_send_mutex[level])
		return 0;
	else
		qspn_send_mutex[level]=1;

	/*
	 * We have to wait the finish of the old qspn_round to start the new one.
	 */
	while((round_ms=qspn_round_left(level)) > 0) {
		debug(DBG_INSANE, "Waiting %dms to send a new qspn_round, lvl:"
				" %d", round_ms, level);
		usleep(round_ms*1000);
		update_qspn_time(level, 0);
	}

	/* 
	 * If, after the above wait, the old saved qspn_id (`qid') it's not the
	 * same of the current it means that we receveid already a new 
	 * qspn_round in this level, so forget about it ;) 
	 */
	if(qid != me.cur_qspn_id[level])
		return 0;
	
	qspn_new_round(level, 0, 0);
	root_node->flags|=QSPN_STARTER;

	upper_gid=me.cur_quadg.gid[upper_level];
	tracer_pkt_build(QSPN_CLOSE, me.cur_qspn_id[level], root_node_pos, /*IDs*/
			 upper_gid,  level,
			 0,          0,         	    0, 		   /*Received tracer_pkt*/
			 0,          0,              	    0, 		   /*bnode_block*/
			 &pkt);						   /*Where the pkt is built*/

	/*... send the qspn_opened to our r_nodes*/
	tracer_pkt_send(exclude_from_and_glevel_and_closed, upper_gid, 
			upper_level, -1, from, pkt);

	debug(DBG_INSANE, "Qspn_round lvl: %d id: 0x%x sent", level, 
			me.cur_qspn_id[level]);

	qspn_send_mutex[level]=0;
	return ret;
}

/*
 * qspn_open_start: sends a new qspn_open when all the links are closed.
 * `from' is the node who sent the last qspn_close which closed the last 
 * not-closed link. 
 * `pkt_to_all' is the the last qspn_close pkt sent by `from'. It must be passed
 * with the new tracer_pkt entry already added because it is sent as is.
 * `qspn_id', `root_node_pos', `gid' and `level' are the same parameters passed
 * to tracer_pkt_build to build the `pkt_to_all' pkt.
 * This functions is called only by qspn_close().
 */
int qspn_open_start(map_node *from, PACKET pkt_to_all, int qspn_id, int root_node_pos,
		int gid, int level)
{
	PACKET pkt_to_from;
	int upper_level;

	upper_level=level+1;
	
	debug(DBG_INSANE, "Fwd %s(0x%x) lvl %d, to broadcast", rq_to_str(QSPN_OPEN), 
			qspn_id, level);
	
	/* 
	 * The `from' node doesn't need all the previous tracer_pkt entry (which
	 * are kept in `pkt_to_all'), so we build a new tracer_pkt only for it.
	 */
	tracer_pkt_build(QSPN_OPEN, qspn_id, root_node_pos, gid, level,  0, 0, 
			0, 0, 0, 0, &pkt_to_from);	

	/* Send the pkt to `from' */
	tracer_pkt_send(exclude_all_but_notfrom, gid, upper_level, -1, from,
			pkt_to_from);
	
	/* Send the `pkt_to_all' pkt to all the other rnodes */
	pkt_to_all.hdr.op=QSPN_OPEN;
	tracer_pkt_send(exclude_from_glevel_and_setreplied, gid, upper_level, -1, 
			from, pkt_to_all);

	return 0;
}


/* 
 * qspn_close: It receive a QSPN_CLOSE pkt, analyzes it, stores the routes,
 * closes the rpkt.from link and then keeps forwarding it to all the non 
 * closed links. If all the links are closed, a qspn_open will be sent.
 */
int qspn_close(PACKET rpkt)
{
	PACKET pkt;
	brdcast_hdr  *bcast_hdr;
	tracer_hdr   *trcr_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	size_t bblock_sz=0, old_bblock_sz;
	int i, not_closed=0, ret=0, ret_err, left;
	u_int hops;
	u_short old_bchunks=0;
	const char *ntop;
	char *old_bblock;

	map_node *from, *root_node, *tracer_starter, *node;
	map_gnode *gfrom, *gtracer_starter;
	struct timeval trtt;
	void *void_map;
	int gid, root_node_pos;
	u_char level, upper_level;

	if(server_opt.dbg_lvl) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_INSANE, "%s(0x%x) from %s", rq_to_str(rpkt.hdr.op),
				rpkt.hdr.id, ntop);
	}
	
	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &trcr_hdr, &tracer, &bhdr,
			&bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_close(): The %s node sent an invalid "
				"qspn_close pkt here.", ntop);
		return -1;
	}
	gid=bcast_hdr->g_node;
	upper_level=level=bcast_hdr->level;
	hops=trcr_hdr->hops;

#ifdef DEBUG
	if(bcast_hdr->flags & BCAST_TRACER_STARTERS)
		
	debug(DBG_INSANE, "QSPN_FROM: starter : node[0]: %d, node[1]: %d, hops: %d	<<<<", tracer[0].node, 
			trcr_hdr->hops > 1 ? tracer[1].node : -1 ,
			trcr_hdr->hops);
	else
	debug(DBG_INSANE, "QSPN_FROM::: node[0]: %d, node[1]: %d, hops: %d", tracer[0].node, 
			trcr_hdr->hops > 1 ? tracer[1].node : -1 ,
			trcr_hdr->hops);
#endif

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

	from->flags&=~QSPN_OLD;
	
	if(tracer_starter == root_node) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_close(): Dropped qspn_close from "
				"%s: we are the qspn_starter of that pkt! (hops: %d)", 
				ntop, trcr_hdr->hops);
		return 0;
	}
	
	if(rpkt.hdr.id != me.cur_qspn_id[level]) {
		if((left=qspn_round_left(level)) > QSPN_WAIT_DELTA_MS ||
				rpkt.hdr.id < me.cur_qspn_id[level]+1) {
			ntop=inet_to_str(rpkt.from);
			debug(DBG_NOISE, "qspn_close(): The %s sent a qspn_close"
					" with a wrong qspn_id (0x%x) lvl %d", ntop, 
					rpkt.hdr.id, level);
			debug(DBG_INSANE, "qspn_close(): cur_qspn_id: %d, still left %dms", 
					me.cur_qspn_id[level], left);
			return -1;
		} else {
			tracer_get_trtt(root_node, from, trcr_hdr, tracer, &trtt);
			debug(DBG_NOISE, "New qspn_round 0x%x received, "
					"new qspn_time: %dms",
					rpkt.hdr.id, MILLISEC(trtt));
			qspn_new_round(level, rpkt.hdr.id, &trtt);
		}
	}

	/* Time to update our maps */
	tracer_store_pkt(void_map, level, trcr_hdr, tracer, (void *)bhdr, bblock_sz,
			&old_bchunks, &old_bblock, &old_bblock_sz);

	if(hops > 1 && (root_node->flags & QSPN_STARTER) &&
			!(from->flags & QSPN_STARTER)) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_close(): Dropped qspn_close from %s: we"
				" are a qspn_starter, the pkts has (hops=%d)>1"
				" and was sent by a non qspn_starter",
				ntop, hops);
		return 0;
	}

	/*
	 * Only if we are in the level 0, or if we are a bnode, we can do the real
	 * qspn actions, otherwise we simply forward the pkt.
	 */
	not_closed=0;
	if(!level || (root_node->flags & MAP_BNODE)) {
		/*
		 * We close the from node and we see there are any links still 
		 * `not_closed'.
		 */
		for(i=0; i<root_node->links; i++) {
			node=(map_node *)root_node->r_node[i].r_node;

			if(root_node->r_node[i].r_node == (u_int *)from) {
				debug(DBG_INSANE, "Closing %x [g]node", node);
				node->flags|=QSPN_CLOSED;
			}

			if(!(node->flags & QSPN_CLOSED))
				not_closed++;
		}
	}

	/* If we are a starter then `from' is starter too */
	if(root_node->flags & QSPN_STARTER) {
		from->flags|=QSPN_STARTER;
		bcast_hdr->flags|=BCAST_TRACER_STARTERS;
	}

	/*We build d4 p4ck37...*/
	tracer_pkt_build(QSPN_CLOSE, rpkt.hdr.id, root_node_pos,  /*IDs*/
			 gid,	     level,
			 bcast_hdr,  trcr_hdr,    tracer, 	  /*Received tracer_pkt*/
			 old_bchunks,old_bblock,  old_bblock_sz,  /*bnode_block*/
			 &pkt);					  /*Where the pkt is built*/
	if(old_bblock)
		xfree(old_bblock);

	if(!not_closed && !(node->flags & QSPN_REPLIED) && 
			!(root_node->flags & QSPN_STARTER)) {
		/*
		 * We have all the links closed and we haven't sent a 
		 * QSPN_REPLIED yet, time to diffuse a new qspn_open
		 */
		qspn_open_start(from, pkt, rpkt.hdr.id, root_node_pos, gid, level);
		
	} else if(root_node->flags & QSPN_STARTER) {
		/* We send a normal tracer_pkt limited to the qspn_starter nodes */
		pkt.hdr.op=TRACER_PKT;
		pkt.hdr.id=me.cur_node->brdcast;
		debug(DBG_INSANE, "Fwd %s(0x%x) lvl %d to the qspn starters	<<<<", 
				rq_to_str(pkt.hdr.op),  pkt.hdr.id, level);
		
		tracer_pkt_send(exclude_from_and_glevel_and_notstarter, gid, 
				upper_level, -1, from, pkt);
	} else {
		/* 
		 * Forward the qspn_close to all our r_nodes which are not 
		 * closed!
		 */
		debug(DBG_INSANE, "Fwd %s(0x%x) lvl %d to broadcast", rq_to_str(pkt.hdr.op),
				pkt.hdr.id, level);
		tracer_pkt_send(exclude_from_and_glevel_and_closed, gid, 
				upper_level, -1, from, pkt);
	}

	return ret;
}

int qspn_open(PACKET rpkt)
{
	PACKET pkt;
	brdcast_hdr  *bcast_hdr;
	tracer_hdr   *trcr_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	struct qspn_buffer *qb=0;
	int not_opened=0, ret=0, reply, sub_id, ret_err;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	const char *ntop;
	char *old_bblock;

	map_node *from, *root_node;
	map_gnode *gfrom;
	void *void_map;
	int gid, root_node_pos;
	u_char level, upper_level;

	if(server_opt.dbg_lvl) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "%s(0x%x) from %s", rq_to_str(rpkt.hdr.op),
				rpkt.hdr.id, ntop);
	}

	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &trcr_hdr, &tracer, &bhdr, 
			&bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_open(): The %s sent an invalid qspn_open "
				"pkt here.", ntop);
		return -1;
	}
	
	hops=trcr_hdr->hops;
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

	from->flags&=~QSPN_OLD;
	sub_id=bcast_hdr->sub_id;
	if(sub_id == root_node_pos) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_open(): We received a qspn_open from %s,"
				" but we are the opener.", ntop);
		return 0;
	}

	if(rpkt.hdr.id < me.cur_qspn_id[level]) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "qspn_open(): The %s sent a qspn_open"
				" with a wrong qspn_id (0x%x), cur_id: 0x%x", 
				ntop, rpkt.hdr.id, me.cur_qspn_id[level]);
		return -1;
	}

	/*Time to update our map*/
	tracer_store_pkt(void_map, level, trcr_hdr, tracer, (void *)bhdr, bblock_sz,
			&old_bchunks, &old_bblock, &old_bblock_sz);
	
	/* 
	 * We search in the qspn_buffer the reply which has current sub_id. 
	 * If we don't find it, we add it.
	 */
	qb=qspn_b[level];
	if((reply=qspn_b_find_reply(qb, sub_id)) == -1)
		list_for(qb)
			reply=qspn_b_add(qb, sub_id, 0);

	/*
	 * Only if we are in the level 0, or if we are a bnode, we can do the real 
	 * qspn actions, otherwise we simply forward the pkt.
	 */
	not_opened=0;
	if(!level || (root_node->flags & MAP_BNODE)) {
		/* Time to open the links */
		qb=qspn_b[level];
		list_for(qb) {
			if(qb->rnode == from)
				qb->flags[reply]|=QSPN_OPENED;

			if(!(qb->flags[reply] & QSPN_OPENED))
				not_opened++;
		}
		/*Fokke, we've all the links opened. let's take a rest.*/
		if(!not_opened) {
			debug(DBG_NOISE, "qspn_open(): We've finished the "
					"qspn_open (sub_id: %d) phase", sub_id);
			return 0;
		}
	}

	/* The forge of the packet. "One pkt to rule them all". Dum dum */
	tracer_pkt_build(QSPN_OPEN, rpkt.hdr.id, bcast_hdr->sub_id,/*IDs*/
			 gid, 	    level,
			 bcast_hdr, trcr_hdr, tracer, 		   /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz,   /*bnode_block*/
			 &pkt);					   /*Where the pkt is built*/
	if(old_bblock)
		xfree(old_bblock);

	debug(DBG_INSANE, "%s(0x%x) to broadcast", rq_to_str(pkt.hdr.op),
			pkt.hdr.id);

	/*...forward the qspn_opened to our r_nodes*/
	tracer_pkt_send(exclude_from_and_opened_and_glevel, gid, upper_level, 
			sub_id, from, pkt);
	return ret;
}
