/* This file is part of Netsukuku
 * (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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


/* 
 * NOTE: this file will substitute map.h
 */


#ifndef MAP_H
#define MAP_H

#include "inet.h"

/* Generic map defines */
#define MAXGROUPNODE_BITS	8	/* 2^MAXGROUPNODE_BITS == MAXGROUPNODE */
#define MAXGROUPNODE		(1<<MAXGROUPNODE_BITS)
#define MAXROUTES	 	8

#define MAXLINKS		MAXROUTES

/*
 * map_node.flags 
 */

#define MAP_ME		1	/* The root node, i.e. localhost */
#define MAP_VOID	(1<<1)	/* This node doesn't exist in the net */
#define MAP_HNODE	(1<<2)  /* Hooking node. The node is hooking*/
#define MAP_BNODE	(1<<3)	/* The node is a border_node. */
#define MAP_ERNODE	(1<<4)  /* External rnode */
#define MAP_GNODE	(1<<5)  /* Group node*/
#define MAP_RNODE	(1<<6)	/* A rnode of the root node */
#define MAP_UPDATE	(1<<7)	/* If it is set, the node status changed 
				   since the last update, thus the its
				   route in the krnl should be updated */

/*
 * 		      The QSPN internal map
 * 		    =========================
 *
 * For any node `N' of the map, `N.gw' points to the rnode of the root node
 * to be used as gateway to reach `N'.
 * The only execption is the root_node itself: in `root_node.gw' we keep the 
 * rnodes of the root node.
 * The root node may have also rnodes of a different gnode, i.e. border nodes.
 * To store these external rnodes in root_node.r_node[x], the
 * * TODO: CONTINUE HERE * 
 * root_node.r_node[x].r_node will point to the relative ext_rnode struct 
 * (see gmap.h) and the MAP_GNODE | MAP_ERNODE flags will be set in 
 * root_node.r_node[x].flags.
 * The rnodes of the root_node of 0 level are updated by the radar(), 
 * instead the root_nodes of greater levels are updated by the qspn.
 */

/*
 * TODO: 
 * 	 - link_id
 * 	 - REM
 */

typedef struct
{
	u_int		rtt;		/* Round trip time in ms */

	u_char		upbw;		/* Upload bandwidth */
	/* TODO: remember the bottleneck */
	u_char		dwbw;		/* Download */
	u_char		uptime;		/* TODO: Specify a good format. Use bandwidth_in_8bit ? */
} rem_t;

typedef struct
{
	nid_t		node;		/* TODO: when a `node' dies, we must update 
					   all the `map_node.gw' */
	
	rem_t		rem;
	
	u_int		tphash[2];
	u_char		tpsz;

	/* TODO:
	 * u_char	similarity_with_`gw[0]';
	 *
	 * insertion:
	 *
	 * remotion:
	 *
	 * `similarity_with_' viene anche usata per sortare (escluso gw[0])
	 *
	 * Questa storia non influenza il QSPN
	 *
	 * In gw[0] ci sta' solo chi ha il REM migliore.
	 *
	 * u_int	gwhash;	<-- usato per distinguere rotte uguali e
	 * quindi scartare quella peggiore. E' utile solo nel caso in cui i
	 * rispettivi gw sono diversi (quindi non si deve hashare il gw, ma
	 * solo gli altri hop. Ah, si hasha la bitmask del TP (escluso il gw).
	 *
	 * Come distinguere tra rotte di upload e download?
	 */
} map_gw;

/*
 * Link ID
 * 
 * Its 16 bits are splitted in this way:
 *
 * 	||   node id   |   link id counter   ||
 *	     8 bits           8 bits
 * TODO: what to do when the counter resets?
 */
typedef unsigned short linkid_t;

/* 
 * Node ID
 *
 * It's a number 0 <= n <= 255 
 */
typedef unsigned char nid_t;

#define MAX_LINKS	MAXGROUPNODE

typedef struct
{
	u_short 	flags;

	linkid_t	*linkids;	/* Array of link IDs */ 
	u_char		links;		/* # links of this node */

	map_gw		*gw;		/* Gateways used by the root node to
					   reach this node */
	u_short		gw_counter;	/* Number of gateways*/

	/* 
	 * The Tracer Packet bitmask of map_node.gw[0]
	 * It is formed by MAX_TP_HOPS bits.
	 * If the i-th bit is set, then the node with id `i', was a hop of the 
	 * Tracer Packet from which gw[0] was extracted.
	 *
	 * The `tpmask' is used to discard routes similar to that of gw[0],
	 * thus all the gateways added in `map_node.gw' will point to routes
	 * dissimilar to that of map_node.gw[0].
	 *
	 * In this way, if the route of gw[0] dies, the others will have a
	 * high probability of being active, because they share a low number
	 * of hops.
	 */
	u_char		tpmask[MAX_TP_HOPS/8];

	RSA		*pubk;		/* Public key of the node */
} map_node;

#endif /* MAP_H */
