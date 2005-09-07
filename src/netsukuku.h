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

#define VERSION			"NetsukukuD 0.0.3b"

struct current_globals
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
#define NTK_TCP_PORT		269
#define NTK_UDP_RADAR_PORT	271

#define ANDNA_UDP_PORT 	   	277
#define ANDNA_TCP_PORT		277

int my_family;
u_short ntk_udp_port, ntk_udp_radar_port, ntk_tcp_port;
u_short andna_udp_port, andna_tcp_port;

int ll_map_initialized;

#define NTK_CONFIG_FILE "/etc/netsukuku/netsukuku.conf"

#define INT_MAP_FILE	"ntk_internal_map"
#define EXT_MAP_FILE	"ntk_external_map"
#define BNODE_MAP_FILE	"ntk_bnode_map"

#define ANDNA_HNAMES_FILE	"andna_hostnames"
#define ANDNA_CACHE_FILE	"andna_cache"
#define LCL_FILE		"andna_lcl_cache"
#define RHC_FILE		"andna_rh_cache"
#define COUNTER_C_FILE		"andna_counter_cache"

typedef struct
{
	char		config_file[NAME_MAX];

	char 		dev[IFNAMSIZ];
	int 		family;

	char 		int_map_file[NAME_MAX];
	char 		bnode_map_file[NAME_MAX];
	char 		ext_map_file[NAME_MAX];

	char		andna_hnames_file[NAME_MAX];
	char 		andna_cache_file[NAME_MAX];
	char 		lcl_file[NAME_MAX];
	char		rhc_file[NAME_MAX];
	char 		counter_c_file[NAME_MAX];

	char 		restricted;
	char 		daemon;
	char 		dbg_lvl;

	char		disable_andna;

	int 		max_connections;
	int 		max_accepts_per_host;
	int 		max_accepts_per_host_time;
}ServOpt;
ServOpt server_opt;

/* Just to be sure */
#ifdef QSPN_EMPIRIC
	#error Netsukuku_d cannot be compiled with the QSPN_EMPIRIC support activated.
#endif
