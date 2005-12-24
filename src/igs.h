/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#ifndef IGS_H
#define IGS_H

/*
 * 	* Bandwidth notes *
 * When we talk of `bandwidth' we mean the average of the download and 
 * upload bandwidth of a particular node.
 * The bandwidth of a gnode is the average of all the bandwidths of the nodes
 * belonging to that gnode.
 * 
 * Internally we save the `bandwidth' as a u_char variable using the
 * `bandwidth_in_8bit' function (see igs.c)
 */

/* Minum bandwidth necessary to share an internet connection */
#define MIN_CONN_BANDWIDTH	3		/* 16 Kb/s */

#define MAX_INTERNET_HNAMES	10
#define MAX_INTERNET_HNAME_SZ	64
#define INET_HOST_PING_TIMEOUT	3
#define INET_NEXT_PING_WAIT	5

#define MAXIGWS			MAXGROUPNODE	/* max number of internet 
						   gateways in each level */

/*
 * internet_gateway: this struct points to a particular (g)node which is
 * sharing its Internet connection
 */
struct internet_gateway
{
	struct internet_gateway *next;
	struct internet_gateway *prev;

	u_char		gid;
	map_node	*node;
	
	u_char		bandwidth;	/* Its Internet bandwidth */
};
typedef struct internet_gateway inet_gw;

/* We pack only `gid' and `bandwidth' */
#define INET_GW_PACK_SZ		(sizeof(u_char)*2)

struct inet_gw_pack_hdr
{
	u_char		gws[MAX_LEVELS];/* Number of inet_gws there are in the
					   pack, for each level, minus one */
	u_char		levels;
}_PACKED_;

/* 
 * The inet_gw_pack_body is:
 * 	inet_gw_pack	igw[hdr.gws[i]]	   for all the i that goes from 0 to
 * 					   hdr.levels
 */
#define IGWS_PACK_SZ(hdr)						\
({									\
	size_t _sz; int _pi;						\
	_sz=sizeof(struct inet_gw_pack_hdr);				\
	for(_pi=0; _pi<(hdr)->levels; _pi++)				\
		_sz+=INET_GW_PACK_SZ*((hdr)->gws[_pi]+1);		\
	_sz;								\
})

#define MAX_IGWS_PACK_SZ(levels)	(sizeof(struct inet_gw_pack_hdr) + \
						INET_GW_PACK_SZ*MAXIGWS*(levels))

/*
 * * *  Functions declaration  * * 
 */

u_char bandwidth_in_8bit(u_int x);
int str_to_inet_gw(char *str, inet_prefix *gw, char *dev);
char **parse_internet_hosts(char *str, int *hosts);
void free_internet_hosts(char **hnames, int hosts);

void init_my_igw(void);
void init_igws(inet_gw ***igws, int **igws_counter, int levels);
void reset_igws(inet_gw **igws, int *igws_counter, int levels);
void free_igws(inet_gw **igws, int *igws_counter, int levels);
void init_my_igws(inet_gw **igws, int *igws_counter,
		inet_gw ***my_new_igws, u_char my_bandwidth, 
		map_node *cur_node, quadro_group *qg);
void free_my_igws(inet_gw ***my_igs);
void init_internet_gateway_search(void);
inet_gw *igw_add_node(inet_gw **igws, int *igws_counter,  int level,
		int gid, map_node *node, u_char bandwidth);
int igw_del_node(inet_gw **igws, int *igws_counter,  int level,
		map_node *node);
void igw_update_gnode_bw(int *igws_counter, inet_gw **my_igws, inet_gw *igw,
		int new, int level, int maxlevels);
void igw_order(inet_gw **igws, int *igws_counter, inet_gw **my_igws, int level);

int igw_check_inet_conn(void);
void *igw_check_inet_conn_t(void *null);

int igw_exec_masquerade_sh(char *script);
int igw_replace_default_gateways(inet_gw **igws, int *igws_counter, 
		inet_gw **my_igws, int max_levels, int family);

char *pack_igws(inet_gw **igws, int *igws_counter, int levels, int *pack_sz);
int unpack_igws(char *pack, size_t pack_sz,
		map_node *int_map, map_gnode **ext_map, int levels,
		inet_gw ***new_igws, int **new_igws_counter);

#endif /*IGS_H*/
