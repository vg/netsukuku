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

#define VERSION			"NetsukukuD 0.0.1b"

struct current
{
	map_node  	*int_map;	/*Internal Map*/
	
	map_gnode      **ext_map;	/*External Map. It is ext_map[ext_levels]
					  and each element contains the 
					  entire ext_map for that level*/
	quadro_group     cur_quadg;
	
	map_bnode      **bnode_map;	/*Current boarder nodes map, read bmap.h*/
	u_int 		*bmap_nodes;	/*How many bnodes there are in map_bnode*/
	
	inet_prefix	 cur_ip;
	map_node 	*cur_node;	/*Me in the map*/
	map_rnode	*cur_rnode;	/*At the hooking time we haven't rnodes, 
					  so this will point a stub rnode struct
					  present at cur_node->r_node*/
	ext_rnode_cache *cur_erc;       /*This is the current external rnode cache list (see gmap.h)*/
	u_int		cur_erc_counter;

	int 		*cur_qspn_id;	/*The current qspn_id we are processing. 
					  It is cur_qspn_id[levels] big*/
	struct timeval	*cur_qspn_time; /*When the last qspn round was sent (gettimeofday format).
					  It is cur_qspn_time[levels] big*/

	char 		 cur_dev[IFNAMSIZ];
	int		 cur_dev_idx;
}me;

#define DEFAULT_NTK_UDP_PORT    269
#define DEFAULT_NTK_TCP_PORT    269

int my_family;
u_short ntk_udp_port, ntk_tcp_port;

int ll_map_initialized;

#define INT_MAP_FILE	"ntk_internal_map"
#define EXT_MAP_FILE	"ntk_external_map"
#define BNODE_MAP_FILE	"ntk_bnode_map"

typedef struct
{
	char 		dev[IFNAMSIZ];
	int 		family;

	char 		int_map_file[NAME_MAX];
	char 		bnode_map_file[NAME_MAX];
	char 		ext_map_file[NAME_MAX];

	char 		daemon;
	char 		dbg_lvl;

	int 		max_connections;
	int 		max_accepts_per_host;
	int 		max_accepts_per_host_time;
}NtkOpt;
NtkOpt server_opt;

int init_load_maps(void);
int save_maps(void);
int free_maps(void);
int fill_default_options(void);
void usage(void);
void parse_options(int argc, char **argv);
void init_netsukuku(char **argv);
void destroy_netsukuku(void);
