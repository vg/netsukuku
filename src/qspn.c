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

extern int my_family;
extern struct current me;

int qspn_send(u_char op, u_char bcast_type, u_char reply, int q_id, int q_sub_id)
{
	
}

int qspn_close(PACKET rpkt)
{
	struct brdcast_hdr *bcast_hdr=rpkt.msg;
	struct tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(struct brdcast_hdr);
	struct tracer_node *tracer=rpkt.msg+sizeof(struct brdcast_hdr)+sizeof(struct tracer_hdr);
	inet_prefix to;
	ssize_t err;
	int i;

	if(me.cur_node->flags & QSPN_STARTER && GLI_HOPS_NN_SONO_1)
		return 0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	for(i=0; i<me.cur_node->links; i++) {
		if(qspn_q[i].flags & QSPN_CLOSED)
			continue;
		memset(&to, 0, sizeof(inet_prefix));
		maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to)
		pkt_addto(&pkt, &to);
		pkt.sk_type=SKT_UDP;

		/*Let's send the pkt*/
		err=send_rq(&pkt, 0, QSPN_CLOSE, rpkt.hdr.id, 0, 0, 0);
		pkt_free(&pkt, 1);
		if(err==-1) {
			char *ntop;
			ntop=inet_to_str(&pkt->to);
			error("radard(): Cannot send the QSPN_CLOSE[id: %d] to %s.", qspn_id, ntop);
			xfree(ntop);
		}
	}
}

int qspn_open(PACKET rpkt)
{
}

int qspn_reply(PACKET rpkt)
{
}

int qspn_backpro(PACKET rpkt)
{
}

int qspn_verify_tracer(struct tracer_node *tracer, int hops)
{
	map_node *from;
	int e, x=0;

	/*"from" has to be absolutely one of our rnodes*/
	from=node_from_pos(tracer[hops-1].node, map);
	for(e=0; e<me.cur_node->links; e++)
		if((int)*me.cur_node->r_node[e].r_node == (int)*from) {
			x=1;
			break;
		}
	if(!x) {
		error("The tracer pkt is invalid! We don't have %d as rnode", t[hops-1].node);
		return -1;
	}

	return 0;
}
/* qspn_inc_tracer: Add our entry ("node") to the tracer pkt "tracer wich has "hops".
 * It returns the modified tracer pkt and it increments the *hops */
struct tracer_node *qspn_inc_tracer(map_node *node, struct tracer_node *tracer, int *hops)
{
	struct tracer_node *t;

	if(qspn_verify_tracer(tracer, *hops))
		return 0;
	*hops++;
	t=xrealloc(tracer, sizeof(struct tracer_node)*hops);
	t[hops-1].node=((void *)node-(void *)me.int_map)/sizeof(map_node);
	memcpy(&t[hops-1].rtt, me.cur_node->r_node[e].rtt, sizeof(struct timeval));
	
	return t;
}

/* qspn_store_tracer: This is the main function used to keep the map's karma in peace.
 * It updates the "map" with the given "tracer" pkt
 */
int qspn_store_tracer(map_node *map, map_node *root_node, struct tracer_node *tracer, int hops)
{
	int i, e, diff;
	struct timeval trtt;
	map_node *from, *node;
	
	if(qspn_verify_tracer(tracer, *hops))
		return 0;

	from=node_from_pos(tracer[hops-1].node, map);
	if(!(from.flags & MAP_RNODE)) { /*the `from' node isn't in our r_nodes. Add it!*/
		map_rnode rnn;
		
		memset(&rnn, '\0', sizeof(map_rnode));
		rnn.r_node=from;
		memcpy(&rnn.rtt, &tracer[hops-1].rtt, sizeof(struct timeval));
		rnode_add(root_node, &rnn);

		from->flags|=MAP_RNODE;
		from->flags&=~MAP_VOID & ~MAP_UPDATE;
	}

	timeradd(tracer[0].rtt, &trtt, &trtt);
	for(i=hops-1; i != hops; i--) {
		timeradd(tracer[i].rtt, &trtt, &trtt);
		
		node=node_from_pos(tracer[i].node, map);
		if(!(node->flags & MAP_VOID)) {
			from->flags&=~MAP_VOID;
			from->flags|=MAP_UPDATE;
		}
		for(e=0; e < node->links; e++) {
			if(node->r_node[e].r_node == from) {
			   diff=abs(MILLISEC(node->r_node[e].trtt) - MILLISEC(trtt));
			   if(diff >= RTT_DELTA) {
				   memcpy(&node->r_node[e].trtt, &trtt, sizeof(struct timeval));
				   node->flags|=MAP_UPDATE;
			   }
			   f=1;
			}
		}
		if(!f) { /*If the `node' doesn't have `from' in his r_nodes... let's add it*/
			map_rnode rnn;

			memset(&rnn, '\0', sizeof(map_rnode));
			rnn.r_node=from;
			memcpy(&rnn.rtt, &trtt, sizeof(struct timeval));
			rnode_add(node, &rnn);

			node->flags|=MAP_UPDATE;
		}

		/*ok, now the kernel needs a refresh*/
		if(node->flags & MAP_UPDATE) {
			rnode_trtt_order(node);
			krnl_update_node(node);
		}
	}
}
