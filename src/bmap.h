/* This file is part of Netsukuku system
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

/* map_bnode is the struct used to create the "map boarder node". 
 * This map keeps for each boarder node of the int_map the gnodes which they are linked to.
 * As always there are some little differences from the map_node:
 *
 *	__u16 		links;		is the number of gnodes the bnode is linked to.
 *	map_rnode	*r_node;	r_node[x].r_node, in this case, points to the position of the bnode's gnode in 
 *					the ext_map.
 *	u_int           brdcast;	Where this node is in the int_map. The position is stored in the usual
 *					pos_from_node() format. (Yep, a dirty hack)
 *
 * So you are asking why didn't I made a new struct for the bmap. Well, I don't want to [re]write all the functions 
 * to handle the map, for example rnode_add,rnode_del, save_map, etc... it's a pain, just for a little map and moreover
 * it adds new potential bugs. In conclusion: laziness + fear == hacks++;
 */
typedef map_node map_bnode;
#define MAXGROUPBNODE		MAXGROUPNODE	/*the maximum number of bnodes in a gnode is equal to the maximum 
						  number of nodes*/
#define MAXBNODE_LINKS		0x100		/*The maximum number of gnodes a bnode is linked to*/
#define MAXBNODE_RNODEBLOCK	MAXBNODE_LINKS*MAXGROUPBNODE*sizeof(map_rnode)

/*These defines make the life easier, so instead of writing int_map_hdr I write bnode_map_hdr.
 * Cool eh? ^_^. int_map_hdr is in pkts.h*/
#define bnode_ptr		brdcast		/*Don't kill me*/
#define bnode_map_hdr 		int_map_hdr
#define bnode_map_sz   		int_map_sz

/* * * Functions' declaration * * */
map_bnode *map_bnode_del(map_bnode *bmap, u_int *bmap_nodes,  map_bnode *bnode);
int map_find_bnode(map_node *int_map, map_bnode *bmap, int count, map_node *node);

int save_bmap(map_bnode *bmap, int *gmap, u_int bmap_nodes, char *file);
map_bnode *load_bmap(char *file, int *gmap, u_int *bmap_nodes);
