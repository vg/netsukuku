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
	for(i=0; i<me.cur_node->links; i++) {
		if(qspn_b[i].replies) {
			xfree(qspn_b[i].replier);
			xfree(qspn_b[i].flags);
		}
		memset(&qspn_b[i], 0, sizeof(struct qspn_buffer));
	}
}

void qspn_b_add(u_short rnode, u_short replier, u_short flags)
{
	qspn_b[rnode].replies++;
	qspn_b[rnode].replier=xrealloc(qspn_b[rnode].replier, sizeof(u_short)*qspn_b[rnode].replies);
	qspn_b[rnode].flags=xrealloc(qspn_b[rnode].flags, sizeof(u_short)*qspn_b[rnode].replies);
	
	qspn_b[rnode].replier[qspn_b[rnode].replies-1]=replier;
	qspn_b[rnode].flags[qspn_b[rnode].replies-1]=flags;
}

int qspn_b_find_reply(u_short rnode, int sub_id)
{
	int i;

	for(i=0; i<qspn_b[rnode].replies; i++)
		if(qspn_b[rnode].replies[i]==sub_id)
			return i;
	return -1;
}

/* qspn_round_left: It returns the seconds left before the QSPN_WAIT_ROUND expires.
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

int qspn_send(u_char op, u_char bcast_type, u_char reply, int q_id, int q_sub_id)
{
	/*TODO: qspn_time, sleep, cycle*/
	if(qspn_send_mutex)
		return 0;
	else
		qspn_send_mutex=1;

	me.cur_node->flags|=QSPN_STARTER;
	/*TODO:******************* CONTINUE HERE* *********************+++++
	 *     radar.c has to update the qspn_b!!
	 *     if(I_AM_A_BNODE)
	 *     		do_the_bnode_block_stuff();*/
	qspn_send_mutex=0;
}


int qspn_close(PACKET rpkt)
{
	PACKET pkt;
	struct brdcast_hdr *bcast_hdr=rpkt.msg;
	struct tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(struct brdcast_hdr);
	struct tracer_chunk *tracer=rpkt.msg+sizeof(struct brdcast_hdr)+sizeof(struct tracer_hdr);
	struct tracer_chunk *new_tracer=0;
	struct bnode_hdr    *bhdr=0;
	inet_prefix to;
	map_node *from;
	ssize_t err, bblock_sz=0, tracer_sz=0;
	int i, not_closed=0, hops, ret=0;
	char *ntop;

	if(me.cur_node->flags & QSPN_STARTER) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_close(): We received a qspn_close from %s, but we are the QSPN_STARTER.", ntop);
		xfree(ntop);
		return 0;
	}

	tracer_sz=(sizeof(struct tracer_hdr)+(tracer_hdr.hops*sizeof(struct tracer_chunk))+sizeof(struct brdcast_hdr));
	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=rpkt.msg+rpkt.hdr.sz-tracer_sz;
	}
	if(bcast_hdr->g_node != me.cur_gid || rpkt.hdr.sz != (tracer_sz-bblock_sz)) {
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
		} else { /*New round activated. Destroy the old one. beep.*/
			me.cur_qspn_id++;
			update_qspn_time();
			qspn_b_clean();
			me.cur_node->flags&=~QSPN_STARTER;
			for(i=0; i<me.cur_node->links; i++)
				me.cur_node->r_node[i].r_node.flags&=~QSPN_CLOSED & ~QSPN_REPLIED;

			/******TODO: How to remove the old node from the map? TODO*****/
		}
	}


	/*Time to update our map*/
	hops=tracer_hdr.hops;
	from=node_from_pos(tracer[hops-1].node);
	tracer_store_pkt(tracer, hops, bhdr, bblock_sz);

	/*Let's add our entry in the tracer*/
	new_tracer=tracer_add_entry(me.cur_node, tracer, &hops);

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
		bcast_hdr->sz=TRACERPKT_SZ(hops);
		pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
		tracer_hdr->hops=hops;
		pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer);

		/*... to send it to all*/
		for(i=0; i<me.cur_node->links; i++) {
			if(me.cur_node->r_node[i].r_node == from || (me.cur_node->r_node[i].r_node & MAP_GNODE))
				continue;
			
			memset(&to, 0, sizeof(inet_prefix));
			maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to);
			pkt_addto(&pkt, &to);
			pkt.sk_type=SKT_UDP;

			/*We shot the qspn_open*/
			err=send_rq(&pkt, 0, QSPN_OPEN, pos_from_node(me.cur_node, me.int_map), 0, 0, 0);
			if(err==-1) {
				ntop=inet_to_str(&pkt->to);
				error("qpsn_close(): Cannot send the QSPN_OPEN[id: %d] to %s.", qspn_id, ntop);
				xfree(ntop);
			}
		}
		/*nothing to do, let's die*/
		ret=0;
		goto finish;
	}

	/*Here we are building the pkt to... */
	memset(&pkt, '\0', sizeof(PACKET));
	bcast_hdr->sz=TRACERPKT_SZ(hops);
	pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	tracer_hdr->hops=hops;
	pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, tracer);
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
			error("qspn_close(): Cannot send the QSPN_CLOSE[id: %d] to %s.", qspn_id, ntop);
			xfree(ntop);
		}
	}
		
finish:
	pkt_free(&pkt, 1);
	if(new_tracer)
		xfree(new_tracer);	
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
	inet_prefix to;
	map_node *from;
	ssize_t err, bblock_sz=0, tracer_sz=0;
	int i, not_opened=0, hops, ret=0, reply, sub_id;
	char *ntop;

	if(bcast_hdr.sub_id == pos_from_node(me.cur_node)) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "qspn_open(): We received a qspn_open from %s, but we are the opener.", ntop);
		xfree(ntop);
		return 0;
	}

	tracer_sz=(sizeof(struct tracer_hdr)+(tracer_hdr.hops*sizeof(struct tracer_chunk))+sizeof(struct brdcast_hdr));
	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=rpkt.msg+rpkt.hdr.sz-tracer_sz;
	}
	if(bcast_hdr->g_node != me.cur_gid || rpkt.hdr.sz != (tracer_sz-bblock_sz)) {
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
	from=node_from_pos(tracer[hops-1].node);

	/*Time to update our map*/
	tracer_store_pkt(tracer, hops, bhdr, bblock_sz);

	/*Let's add our entry in the tracer*/
	new_tracer=tracer_add_entry(me.cur_node, tracer, &hops);

	/* We search in the qspn_buffer the reply which has current sub_id. 
	 * If we don't find it, we add it*/
	if((reply=qspn_b_find_reply(0, sub_id))==-1)
		for(i=0; i<me.cur_node->links; i++)
			qspn_b_add(pos_from_node(me.cur_node->r_node[i].r_node), sub_id, 0);

	/*Time to open the links*/
	for(i=0; i<me.cur_node->links; i++) {
		if(me.cur_node->r_node[i].r_node == from)
			qspn_b[i].flags[reply]|=QSPN_OPENED;

		if(!(me.cur_node->r_node[i].r_node[reply] & QSPN_OPENED))
			not_opened++;
	}
	/*Fokke, we've all the links opened. let's take a rest.*/
	if(!not_opened && !(qspn_b[i].flags & QSPN_REPLIED)) {
		debug(DBG_NOISE, "qspn_open(): We've finished the qspn_open (sub_id: %d) phase", sub_id);
		return 0;
	}

	/*Here we are building the pkt to... */
	memset(&pkt, '\0', sizeof(PACKET));
	bcast_hdr->sz=TRACERPKT_SZ(hops);
	pkt.hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	tracer_hdr->hops=hops;
	pkt.msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer);
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
			error("qspn_open(): Cannot send the QSPN_OPEN[id: %d] to %s.", qspn_id, ntop);
			xfree(ntop);
		}
	}
		
finish:
	pkt_free(&pkt, 1);
	if(new_tracer)
		xfree(new_tracer);	
	return ret;
}
