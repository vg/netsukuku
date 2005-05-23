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

/* 
 * map_bnode is the struct used to create the "map boarder node". 
 * This map keeps all the boarder node of the map, making it easy to retrieve
 * the gnode they are linked to.
 * It is indentical to the map_node but, as always there are some little 
 * differences:
 *
 *	__u16 		links;		is the number of gnodes the bnode is 
 *					linked to.
 *	map_rnode	*r_node;	r_node[x].r_node, in this case, points 
 *					to the position of the bnode's gnode in
 *					the ext_map.
 *	u_int           brdcast;	Where this node is in the int/ext_map.
 *					The position is stored in the usual
 *					pos_from_node() format. (Yep, a dirty hack)
 *
 * So you are asking why didn't I made a new struct for the bmap. Well, I don't 
 * want to [re]write all the functions to handle the map, for example 
 * rnode_add,rnode_del, save_map, etc... it's a pain, just for a little map and 
 * moreover it adds new potential bugs. In conclusion: laziness + fear == hacks++;
 */
typedef map_node map_bnode;
#define MAXGROUPBNODE		MAXGROUPNODE	/*the maximum number of bnodes in 
						  a gnode is equal to the maximum 
						  number of nodes*/
#define MAXBNODE_LINKS		(MAXGROUPNODE*2)/*The maximum number of gnodes a
						  bnode is linked to*/
#define MAXBNODE_RNODEBLOCK	(MAXBNODE_LINKS*MAXGROUPBNODE*sizeof(map_rnode))

/* 
 * These defines make the life easier, so instead of writing int_map_hdr I
 * write bnode_map_hdr. Cool eh? ^_^.
 */
#define bnode_ptr		brdcast		/*Don't kill me*/
#define bnode_map_hdr 		int_map_hdr
#define bnode_map_sz   		int_map_sz


/* 
 * boarder node block: this is the block which keeps the gnodes linked to the 
 * `bnode' boarder_node. When a bnode has to add his entry in the tracer_pkt it 
 * encapsulates the bnode_block at the end of the packet, in this way it is 
 * possible to know all the gnodes linked to the bnode's gnode.
 * Note: It is possible that the packet passes trough many bnodes, in this case 
 * the bnode block is always put at the end, ex: 
 * |pkt_hdr|brdcast_hdr|tracer_hdr|tracer_chunks|bnode_hdr|bnode_chunks|bnode_hdr|bnode_chunks|...
 * and so on.
 */
typedef struct
{
	u_char bnode;		/*The bnode this bnode_block belongs to*/
	u_short links;		/*The number of linked gnode*/
}_PACKED_ bnode_hdr;

typedef struct
{
	/* The `bnode_hdr.bnode' boards with the `gnode' of `level'th level with
	 * a round trip time which is stored in `rtt'. */

	u_char gnode;	     
	u_char level;
	struct timeval rtt;
}_PACKED_ bnode_chunk;
#define BNODEBLOCK_SZ(links) (sizeof(bnode_hdr)+(sizeof(bnode_chunk)*(links)))


/* 
 * This is the header placed on top of all the bnode_map blocks.
 * So the bnode maps final block is:
 * 	bnode_maps_hdr
 * 	---------
 * 	bnode_map_hdr
 * 	bnode_map_block
 * 	---------
 * 	bnode_map_hdr
 * 	bnode_map_block
 * 	---------
 * 	...
 */
struct bnode_maps_hdr
{
	u_char levels;
	size_t bmaps_block_sz;
}_PACKED_;

/* * * Functions' declaration * * */
void bmap_level_init(u_char levels, map_bnode ***bmap, u_int **bmap_nodes);
void bmap_level_free(map_bnode **bmap, u_int *bmap_nodes);

int map_add_bnode(map_bnode **bmap, u_int *bmap_nodes, u_int bnode, u_int links);
map_bnode *map_bnode_del(map_bnode *bmap, u_int *bmap_nodes,  map_bnode *bnode);
int map_find_bnode(map_bnode *bmap, int bmap_nodes, void *void_map, int node);
int map_find_bnode_rnode(map_bnode *bmap, int bmap_nodes, void *n);

char *pack_all_bmaps(map_bnode **, u_int *, map_gnode **, quadro_group, size_t *);
map_bnode **unpack_all_bmaps(char *, size_t, u_char, map_gnode **, u_int **, int, int);

int save_bmap(map_bnode **, u_int *, map_gnode **, quadro_group, char *);
map_bnode **load_bmap(char *, map_gnode **, u_char, u_int **);
