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

#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#include "qspn.h"
#include "log.h"
#include "xmalloc.h"
#include "misc.h"

extern int my_family;
extern struct current me;

void qspn_b_clean(void)
{
	int i;
	struct qspn_buffer *qb=qspn_b;
	list_for(qb) {
		if(qb->replies) {
			xfree(qb->replier);
			xfree(qb->flags);
		}
		memset(qb, 0, sizeof(struct qspn_buffer));
	}
}

void qspn_b_add(struct qspn_buffer *qb, u_short replier, u_short flags)
{
	qb->replies++;
	qb->replier=xrealloc(qb->replier, sizeof(u_short)*qb->replies);
	qb->flags=xrealloc(qb->flags, sizeof(u_short)*qb->replies);
	
	qb->replier[qb->replies-1]=replier;
	qb->flags[qb->replies-1]=flags;
}

int qspn_b_find_reply(struct qspn_buffer *qb, int sub_id)
{
	int i;

	for(i=0; i<qb->replies; i++)
		if(qb->replies[i]==sub_id)
			return i;
	return -1;
}

/* qspn_round_left: It returns the milliseconds left before the QSPN_WAIT_ROUND expires.
 * If the returned value is <= 0 the QSPN_WAIT_ROUND is expired.*/
int qspn_round_left(void)
{
	struct timeval cur_t, t;
	
	gettimeofday(&cur_t, 0);
	timersub(&cur_t, &me.cur_qspn_time, &t);
	return QSPN_WAIT_ROUND_MS - MILLISEC(t);
}


/* update_qspn_time: It updates me.cur_qspn_time;
 * Oh, sorry this code doesn't show consideration for the relativity time shit. So
 * you can't move at a velocity near the light's speed. I'm sorry.
 */
void update_qspn_time(void)
{
	struct timeval cur_t, t;
	int ret;

	gettimeofday(&cur_t, 0);
	timersub(&cur_t, &me.cur_qspn_time, &t);
	ret=QSPN_WAIT_ROUND_MS - MILLISEC(t);

	if(ret < 0 && abs(ret) > QSPN_WAIT_ROUND_MS) {
		ret=ret-(QSPN_WAIT_ROUND_MS*(ret/QSPN_WAIT_ROUND_MS));
		t.tv_sec=ret/1000;
		t.tv_usec=(ret - (ret/1000)*1000)*1000
		timesub(&cur_t, &t, &me.cur_qspn_time);
	}
}

/* qspn_new_round: It prepares all the buffers for the new qspn_round and removes
 * the QSPN_OLD nodes from the int_map.
 */
void qspn_new_round(void)
{
	int bm;
	/*New round activated. Destroy the old one. beep.*/
	me.cur_qspn_id++;
	update_qspn_time();
	qspn_b_clean();
	me.cur_node->flags&=~QSPN_STARTER;
	for(i=0; i<me.cur_node->links; i++)
		me.cur_node->r_node[i].r_node.flags&=~QSPN_CLOSED & ~QSPN_REPLIED;

	/*How to remove the dead nodes from the map? How do we know what are deads?
	 *Pretty simple, we can't know so we wait until the next qspn_round to break them
	 *if they didn't show in the while.
	 */
	for(i=0; i<MAXGROUPNODE; i++) {
		if((me.int_map[i].flags & QSPN_OLD) && !(me.int_map[i].flags & MAP_VOID)) {
			if((me.int_map[i].flags & MAP_BNODE)) {
				if((bm=map_find_bnode(me.bnode_map, me.bmap_nodes, &me.int_map[i]))!=-1)
					me.bnode_map=map_bnode_del(me.bnode_map, &me.bmap_nodes, &me.bnode_map[bm]);
			}
			map_node_del(&me.int_map[i]);
		}
		me.int_map[i].flags|=MAP_OLD;
	}
}

/* The Holy qspn_send. It is used to send a new qspn_round when something changes around the 
 * root_node (me).
 */
int qspn_send(u_char op, u_char bcast_type, u_char reply)
{
	PACKET void_pkt;
	int round_ms, ret;
	
	if(qspn_send_mutex)
		return 0;
	else
		qspn_send_mutex=1;

	/*We have to wait the the finish of the old qspn_round to start the new one ^_-*/
	while((round_ms=qspn_round_left()) > 0) {
		usleep(round_ms);
		update_qspn_time();
	}
	memset(&void_pkt, 0, sizeof(PACKET));
	
	qspn_new_round();
	me.cur_node->flags|=QSPN_STARTER;
	ret=qspn_close(void_pkt);
	qspn_send_mutex=0;
	return ret;
}


int qspn_close(PACKET rpkt)
{
	PACKET pkt;
	struct brdcast_hdr *bcast_hdr=rpkt.msg;
	struct tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(struct brdcast_hdr);
	struct tracer_chunk *tracer=rpkt.msg+sizeof(struct brdcast_hdr)+sizeof(struct tracer_hdr);
	struct tracer_chunk *new_tracer=0;
	struct bnode_hdr    *bhdr=0;
	struct bnode_hdr    *new_bhdr=0;
	struct bnode_chunk  *new_bchunk=0;
	inet_prefix to;
	map_node *from;
	ssize_t err, bblock_sz=0, tracer_sz=0;
	int i, not_closed=0, hops, ret=0, bchunks=0, new_qspn_close=0;
	char *ntop;

	if(!rpkt.op && !rpkt.sz && !rpkt.id && !rpkt.ntk_id[0]) {
		/* Okey, this is an ugly hack but I had to do it. qspn_send calls qspn_close with rpkt 
		 * filled with zeros, in this way qspn_close knows it must send a brand new qspn_close.
		 * In normal situation qspn_close is called by pkt_exec, so the rpkt isn't void.
		 */
		new_qspn_close=1;
		rpkt.hdr.id=me.cur_qspn_id;
		bcast_hdr=xmalloc(sizeof(struct brdcast_hdr));
		memset(bcast_hdr, 0, sizeof(struct brdcast_hdr));
		bcast_hdr.g_node=me.cur_gid;
		bcast_hdr.gttl=1;
		tracer_hdr=xmalloc(sizeof(struct tracer_hdr));
		memset(tracer_hdr, 0, sizeof(struct tracer_hdr));
		from=me.cur_node;
		tracer=0;
		hops=0;
		goto skip_pkt_checks;
	}
		
	if(me.cur_node->flags & QSPN_STARTER) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_close(): We received a qspn_close from %s, but we are the QSPN_STARTER.", ntop);
		xfree(ntop);
		return 0;
	}

	tracer_sz=(sizeof(struct tracer_hdr)+(tracer_hdr->hops*sizeof(struct tracer_chunk))+sizeof(struct brdcast_hdr));
	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=rpkt.msg+rpkt.hdr.sz-tracer_sz;
	}
	if(bcast_hdr->g_node != me.cur_gid) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_close(): The %s node sent an invalid qspn_close pkt here.", ntop);
		xfree(ntop);
		return -1;
	}
	if(tracer_verify_pkt(tracer, tracer_hdr->hops))
		return -1;

	if(rpkt.hdr.id != me.cur_qspn_id) {
		if(qspn_round_left() > 0 || rpkt.hdr.id != me.cur_qspn_id+1) {
			ntop=inet_to_str(&rpkt->from);
			debug(DBG_NOISE, "qspn_close(): The %s sent a qspn_close with a wrong qspn_id");
			xfree(ntop);
		} else
			qspn_new_round();
	}

	/*Time to update our map*/
	hops=tracer_hdr.hops;
	from=node_from_pos(tracer[hops-1].node, me.int_map);
	tracer_store_pkt(me.int_map, tracer_hdr, tracer, hops, bhdr, bblock_sz);

skip_pkt_checks:
	/*Let's add our entry in the tracer*/
	new_tracer=tracer_add_entry(me.int_map, me.cur_node, tracer, &hops);
	/*If we are a bnode we have to add the bnode_block too.*/
	if(me.cur_node->flags & MAP_BNODE)
		if((new_bhdr=tracer_build_bentry(me.int_map, me.cur_node, &new_bchunk)))
			tracer_hdr->bblocks=bchunks=new_bhdr->links;

	for(i=0; i<me.cur_node->links; i++) {
		if(me.cur_node->r_node[i].r_node == from)
			me.cur_node->r_node[i].r_node.flags|=QSPN_CLOSED;

		if(!(me.cur_node->r_node[i].r_node & QSPN_CLOSED))
			not_closed++;
	}
	/*We have all the links closed, time to diffuse a new qspn_open*/
	if(!not_closed && !(me.cur_node->r_node[i].r_node.flags & QSPN_REPLIED)) {
		me.cur_node->r_node[i].r_node.flags|=QSPN_REPLIED;

		/*We build d4 p4ck37... */
		memset(&pkt, '\0', sizeof(PACKET));
		bcast_hdr->sz=TRACERPKT_SZ(hops)+(bchunks ? BNODEBLOCK_SZ(bchunks) : 0);
		bcast_hdr->sub_id=pos_from_node(me.cur_node, me.int_map);
		pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
		tracer_hdr->hops=hops;
		pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer, new_bhdr, new_bchunk);

		/*... to send it to all*/
		for(i=0; i<me.cur_node->links; i++) {
			if(me.cur_node->r_node[i].r_node == from || (me.cur_node->r_node[i].r_node & MAP_GNODE))
				continue;
			
			memset(&to, 0, sizeof(inet_prefix));
			maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to);
			pkt_addto(&pkt, &to);
			pkt.sk_type=SKT_UDP;

			/*We shot the qspn_open*/
			err=send_rq(&pkt, 0, QSPN_OPEN, rpkt.hdr.id, 0, 0, 0);
			if(err==-1) {
				ntop=inet_to_str(&pkt->to);
				error("qpsn_close(): Cannot send the QSPN_OPEN(id: %d, sub_id: %d) to %s.", pkt->hdr.id, 
						bcast_hdr->sub_id, ntop);
				xfree(ntop);
			}
		}
		/*nothing to do, let's die*/
		ret=0;
		goto finish;
	}

	/*Here we are building the pkt to... */
	memset(&pkt, '\0', sizeof(PACKET));
	bcast_hdr->sz=TRACERPKT_SZ(hops)+(bchunks ? BNODEBLOCK_SZ(bchunks) : 0);
	pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	tracer_hdr->hops=hops;
	pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer, new_bhdr, new_bchunk);
	
	/*... forward the qspn_close to our r_nodes*/
	for(i=0; i<me.cur_node->links; i++) {
		if(qspn_q[i].flags & QSPN_CLOSED || me.cur_node->r_node[i].r_node == from || \
				(me.cur_node->r_node[i].r_node & MAP_GNODE))
			continue;
		
		memset(&to, 0, sizeof(inet_prefix));
		maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to);
		pkt_addto(&pkt, &to);
		pkt.sk_type=SKT_UDP;

		/*Let's send the pkt*/
		err=send_rq(&pkt, 0, QSPN_CLOSE, rpkt.hdr.id, 0, 0, 0);
		if(err==-1) {
			ntop=inet_to_str(&pkt->to);
			error("qspn_close(): Cannot send the QSPN_CLOSE[id: %d] to %s.", pkt->hdr.id, ntop);
			xfree(ntop);
		}
	}
		
finish:
	pkt_free(&pkt, 1);
	if(new_tracer)
		xfree(new_tracer);	
	if(new_bhdr)
		xfree(new_bhdr);
	if(new_bchunk)
		xfree(new_bchunk);
	if(new_qspn_close) {
		xfree(bcast_hdr);
		xfree(tracer_hdr);
	}
	return ret;
}

int qspn_open(PACKET rpkt)
{
	PACKET pkt;
	struct brdcast_hdr *bcast_hdr=rpkt.msg;
	struct tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(struct brdcast_hdr);
	struct tracer_chunk *tracer=rpkt.msg+sizeof(struct brdcast_hdr)+sizeof(struct tracer_hdr);
	struct tracer_chunk *new_tracer=0;
	struct bnode_hdr    *bhdr=0;
	struct bnode_hdr    *new_bhdr=0;
	struct bnode_chunk  *new_bchunk=0;
	inet_prefix to;
	map_node *from;
	struct qspn_buffer *qb=qspn_b;
	ssize_t err, bblock_sz=0, tracer_sz=0, bchunks=0;
	int i, not_opened=0, hops, ret=0, reply, sub_id;
	char *ntop;

	if(bcast_hdr.sub_id == pos_from_node(me.cur_node, me.int_map)) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_open(): We received a qspn_open from %s, but we are the opener.", ntop);
		xfree(ntop);
		return 0;
	}

	tracer_sz=(sizeof(struct tracer_hdr)+(tracer_hdr->hops*sizeof(struct tracer_chunk))+sizeof(struct brdcast_hdr));
	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=rpkt.msg+rpkt.hdr.sz-tracer_sz;
	}
	if(bcast_hdr->g_node != me.cur_gid) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_open(): The %s sent an invalid qspn_open pkt here.", ntop);
		xfree(ntop);
		return -1;
	}
	if(tracer_verify_pkt(tracer, tracer_hdr->hops))
		return -1;

	if(rpkt.hdr.id != me.cur_qspn_id) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_open(): The %s sent a qspn_open with a wrong qspn_id");
		xfree(ntop);
	}

	/*Various  abbreviations*/
	sub_id=buffer_hdr.sub_id;
	hops=tracer_hdr.hops;
	from=node_from_pos(tracer[hops-1].node, me.int_map);

	/*Time to update our map*/
	tracer_store_pkt(me.int_map, tracer_hdr, tracer, hops, bhdr, bblock_sz);

	/*Let's add our entry in the new tracer*/
	new_tracer=tracer_add_entry(me.int_map, me.cur_node, tracer, &hops);
	/*If we are a bnode we have to add the bnode_block too.*/
	if(me.cur_node->flags & MAP_BNODE)
		if((new_bhdr=tracer_build_bentry(me.int_map, me.cur_node, &new_bchunk)))
			new_tracer->bblocks=bchunks=new_bhdr->links;

	/* We search in the qspn_buffer the reply which has current sub_id. 
	 * If we don't find it, we add it*/
	if((reply=qspn_b_find_reply(qspn_b, sub_id))==-1) {
		qb=qspn_b;
		list_for(qb)
			if(qb->rnode == from)
				qspn_b_add(qb, sub_id, 0);
	}

	/*Time to open the links*/
	qb=qspn_b;
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

	/*Here we are building the pkt to... */
	memset(&pkt, '\0', sizeof(PACKET));
	bcast_hdr->sz=TRACERPKT_SZ(hops)+(bchunks ? BNODEBLOCK_SZ(bchunks) : 0);
	pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	tracer_hdr->hops=hops;
	pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer, new_bhdr, new_bchunk);
	/*... forward the qspn_opened to our r_nodes*/
	for(i=0; i<me.cur_node->links; i++) {
		if(qspn_q[i].flags & QSPN_OPENED || me.cur_node->r_node[i].r_node == from || \
				(me.cur_node->r_node[i].r_node & MAP_GNODE))
			continue;
		
		memset(&to, 0, sizeof(inet_prefix));
		maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to);
		pkt_addto(&pkt, &to);
		pkt.sk_type=SKT_UDP;

		/*Let's send the pkt*/
		err=send_rq(&pkt, 0, QSPN_OPEN, rpkt.hdr.id, 0, 0, 0);
		if(err==-1) {
			ntop=inet_to_str(&pkt->to);
			error("qspn_open(): Cannot send the QSPN_OPEN[id: %d] to %s.", pkt->hdr.id, ntop);
			xfree(ntop);
		}
	}
		
finish:
	pkt_free(&pkt, 1);
	if(new_tracer)
		xfree(new_tracer);	
	return ret;
}
