/* This file is part of Netsukuku
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published 
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

#ifndef QSPN_H
#define QSPN_H

#include "gmap.h"

#define QSPN_WAIT_ROUND 	32	/*This is a crucial value. It is the number of 
					  seconds to be waited before the next qspn_round 
					  can be sent*/
#define QSPN_WAIT_ROUND_MS	QSPN_WAIT_ROUND*1000
#define QSPN_WAIT_DELTA_MS	64	/*If a qspn_round is sent while 
					  qspn_round_left() < QSPN_WAIT_DELTA_MS,
					  then it is acceptable*/

#ifdef DEBUG
#undef QSPN_WAIT_ROUND
#define QSPN_WAIT_ROUND		8
#endif

/*Wait time bound to a specific level:	y = (w/2)*x  + w/(x+1) */
#define QSPN_WAIT_ROUND_LVL(level) ((level)*(QSPN_WAIT_ROUND/2) + 	       \
					QSPN_WAIT_ROUND/((level)+1))
#define QSPN_WAIT_ROUND_MS_LVL(level) (QSPN_WAIT_ROUND_LVL(level)*1000)

/* The delta grows in this way:  y = x*(w/2) + 2*w*x + w; */
#define QSPN_WAIT_DELTA_MS_LVL(level) ((level)*(QSPN_WAIT_DELTA_MS/2) +	       \
		2*QSPN_WAIT_DELTA_MS*(level) + QSPN_WAIT_DELTA_MS)



/*we are using the qspn_map style II*/
#define QMAP_STYLE_II
#undef  QMAP_STYLE_I
/*****) The qspn int_map (****
 * The struct it's identical to the normal int_map but there are a few
 * differences of meaning in the qmap:
 * We distinguish from qspn_map styleI and styleII.
 * In the styleI:
 * - All the nodes have map_node.r_node which points to the r_node that is part of the 
 *   route to reach the root_node. So from all the nodes it is possible to reach the
 *   root_node following recursively the r_nodes. 
 *   The only execption is the root_node itself. The root_node's map_node.r_node keeps
 *   all its rnodes as a normal (non qspn) map would.
 * - map_node.r_node.rtt is the round trip time needed to reach map_node.r_node[x].r_node 
 *   from map_node. 
 * 
 * Instead in the qspn_map styleII:
 * - map_node.r_node points to the r_node of the root_node to be used as gateway to 
 *   reach map_node. So map_node.r_node stores only the gateway needed to reach map_node
 *   from the root_node.
 *   The only execption is the root_node itself. The root_node's map_node.r_node keeps
 *   all its rnodes as a normal (non qspn) map would.
 * - map_node.r_node.rtt isn't used.
 *
 * The only exception is the root_node. Its rnodes have a different meaning: they are
 * its effective rnodes, so each map_node.r_node points to the node which is the
 * real rnode of the root_node.
 * The root_node at level 0 may have also rnode of a different gnode (it is a border node).
 * To store these external rnodes in root_node.r_node[x], the root_node.r_node[x].r_node 
 * will point to the relative ext_rnode struct (see gmap.h) and the MAP_GNODE | MAP_ERNODE
 * flags will be set in root_node.r_node[x].flags.
 * The rnodes of the root_node of 0 level are updated by the radar(), instead the root_nodes
 * of greater levels are updated by the qspn.
 *
 * Currently the qspn_map styleII is used.
 * typedef qmap_node *int_map;
 */


/* This list keeps tracks of the qspn_pkts sent or
 * received by our rnodes*/
struct qspn_buffer
{	
	struct qspn_buffer *next;
	struct qspn_buffer *prev;
	
	map_node      *	rnode;		/* the rnode this buf is referring to */
	u_int	 	replies;	/* How many replies we forwarded/sent
					   to `rnode' */
	u_char	      * replier;	/* Who has sent these replies (qspn_sub_id) */
	u_short	      * flags;
};
struct qspn_buffer **qspn_b; /*It is sizeof(struct qspn_buffer *)*levels big*/

int *qspn_send_mutex;	     /*It is sizeof(int)*levels big.*/

/*
 * qspn_gnode_count[x] is the number of nodes present in the gnode
 * me.cur_quadg.gnode[x], it is updated at each qspn_round.
 */ 
u_int qspn_gnode_count[IPV4_LEVELS-ZERO_LEVEL];

void qspn_time_reset(int start_level, int end_level, int levels);
void qspn_init(u_char levels);
void qspn_set_map_vars(u_char level, map_node **map, map_node **root_node, 
		int *root_node_pos, map_gnode **gmap);

void qspn_b_clean(u_char level);
int  qspn_b_add(struct qspn_buffer *qb, u_char replier, u_short flags);
int  qspn_b_find_reply(struct qspn_buffer *qb, int sub_id);
struct qspn_buffer *qspn_b_find_rnode(struct qspn_buffer *qb, map_node *rnode);
int qspn_b_del_dead_rnodes(struct qspn_buffer **qb, map_node *root_node);
void qspn_b_del_all_dead_rnodes(void);

int  qspn_round_left(u_char level);
void update_qspn_time(u_char level, struct timeval *);

void qspn_new_round(u_char level, int new_qspn_id, struct timeval *new_qspn_time);

int  qspn_send(u_char level);
int  qspn_close(PACKET rpkt);
int  qspn_open(PACKET rpkt);

#endif /*QSPN_H*/
