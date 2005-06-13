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
	/* int/ext maps */
	map_node  	*int_map;	/*Internal Map*/
	
	map_gnode      **ext_map;	/*External Map. */
	quadro_group     cur_quadg;
	
	/* border nodes maps.(bmap.h) */
	map_bnode      **bnode_map;
	u_int 		*bmap_nodes;		/* bnode counter for each map*/
	u_int		*bmap_nodes_closed;	/* number of closed bnodes   */
	u_int		*bmap_nodes_opened;	/*   "     " opened   "      */
	
	/* Me ^_- */
	inet_prefix	 cur_ip;
	map_node 	*cur_node;

	/* external rnode cache list. (see gmap.h) */
	ext_rnode_cache *cur_erc;
	u_int		cur_erc_counter;

	/* Current Qspn id and qspn time */
	int 		*cur_qspn_id;	/*The current qspn_id we are processing. 
					  It is cur_qspn_id[levels] big*/
	struct timeval	*cur_qspn_time; /*When the last qspn round was received/sent 
					  (gettimeofday format)*/

	char 		 cur_dev[IFNAMSIZ];
	int		 cur_dev_idx;
}me;

#define NTK_UDP_PORT 	   	269
#define NTK_UDP_RADAR_PORT	270
#define NTK_TCP_PORT		269

int my_family;
u_short ntk_udp_port, ntk_udp_radar_port, ntk_tcp_port;

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

	char 		restricted;
	char 		daemon;
	char 		dbg_lvl;

	int 		max_connections;
	int 		max_accepts_per_host;
	int 		max_accepts_per_host_time;
}NtkOpt;
NtkOpt server_opt;
