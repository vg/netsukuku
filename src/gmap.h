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

#include <sys/time.h>
#include <sys/types.h>
#include "map.h"

/***Groupnode stuff***/
#define GMAP_ME		1
#define GMAP_VOID	(1<<1)
#define GMAP_BNODE	(1<<2)
#define GMAP_FULL	(1<<3)		/*The gnode is full!! aaahh, run away!*/

/*Converts an address of a struct in the map to a Group_node Id*/
#define GMAP2GI(mapstart, mapoff)	((((mapoff)-(mapstart))/sizeof(map_gnode)))
/*Converts a Groupnode Id to an address of a struct in the map*/
#define GI2GMAP(mapstart, gi)		(((gi)*sizeof(map_gnode))+(mapstart))

typedef struct
{
        /*The groupnode will cover the range from me.ipstart to me.ipstart+MAXGROUPNODE*/

	__u16 		gid;		/*Gnode Id*/
	__u16		layer;
	__u16 		seeds;		/*The number of active static nodes connected to this gnode.
					  If seeds == MAXGROUPNODE, the gnode is full ^_^*/

	/*Th4 g_m4p starts here. Note that it is a normal map: each node-struct has the pointer
	 * to the nodes connected to it*/
	map_node	g;

	/*In the own g_node entry there are also the boarder_nodes in g_node.r_node*/
}map_gnode;



/* * * Functions' declaration * * */
u_short iptogid(inet_prefix ip);
map_gnode *init_gmap(size_t len);
void free_gmap(map_gnode *map, size_t count);
void set_cur_gnode(u_short gid);
map_rnode *gmap_get_rblock(map_gnode *map, int *count);
int gmap_store_rblock(map_gnode *map, map_rnode *rblock, int count);
