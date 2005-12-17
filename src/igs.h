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

#include "map.h"

/*
 * 	* Bandwidth notes *
 * When we talk of `bandwidth' we mean the average of the download and 
 * upload bandwidth of a particular node.
 * The bandwidth of a gnode is the sum of all the bandwidths of the nodes
 * belonging to that gnode.
 * 
 * Internally we save the `bandwidth' as a u_char variable using the
 * `bandwidth_in_8bit' function (see igs.c)
 */


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

/* * *  Functions declaration  * * */

u_char bandwidth_in_8bit(u_int x);

void init_my_igw(void);
void init_igws(inet_gw ***igws, int **igws_counter, int levels);
void free_igws(inet_gw **igws, int *igws_counter, int levels);
void igw_add_node(inet_gw **igws, int *igws_counter,  int level,
		int gid, map_node *node, u_char bandwidth);
int igw_del_node(inet_gw **igws, int *igws_counter,  int level,
		map_node *node);

void igw_bandwidth_order(inet_gw **igws, int *igws_counter, int level);

int igw_exec_masquerade_sh(char *script);

#endif /*IGS_H*/
