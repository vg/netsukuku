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

#include <sys/types.h>
#include <sys/time.h>
#include "gmap.h"
#include "route.h"

#define DEFAULT_NTK_UDP_PORT    269
#define DEFAULT_NTK_TCP_PORT    269
#define DEFAULT_NTK_PORT_RANGE  10
int ntk_udp_port;
int ntk_tcp_port;

struct current
{
	map_node  	*int_map;	/*Internal Map*/
	/*TODO: Update the ext_map in the src*/
	map_gnode 	**ext_map;	/*External Map. It is ext_map[ext_levels] and each element contains the 
					  entire ext_map for that level*/
	
/*TODO: remove these from the src:
 *	inet_prefix	 ipstart;	The first ip of our gnode (cur_gnode)
 * 	map_gnode 	 *cur_gnode;
 * 	int 		 *cur_gid;	G_node id*/
	quadro_group     cur_quadg;
	
	map_bnode	*bnode_map;	/*Current boarder nodes map, read map.h*/
	u_int 		 bmap_nodes;	/*How many bnodes there are in map_bnode*/
	
	inet_prefix	 cur_ip;
	map_node 	*cur_node;	/*Me in the map*/
	map_rnode	*cur_rnode;	/*At the hooking time we haven't rnodes, so this will point a stub rnode struct
					  present at cur_node->r_node*/

	int 		 cur_qspn_id;	/*The current qspn_id we are processing*/
	struct timeval	 cur_qspn_time;	/*When the last qspn round was sent*/

	rnode_rt 	*cur_rnrt;

	char 		*cur_dev;
	int		 cur_dev_idx;
}me;

int my_family;
int ll_map_initialized=0;

typedef struct
{
	char *dev;
	char *int_map_file;
	char *bnode_map_file;
	char *ext_map_file;
}NtkOpt;
NtkOpt server_opt;

extern char *__argv0;
extern int dbg_lvl;
extern int log_to_stderr;
