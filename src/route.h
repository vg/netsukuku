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

#include <sys/types.h>
#include <asm/types.h>

#define MAX_ROUTE_TABLES	253

struct ctable_route
{
	struct ctable_route	*next;
	struct ctable_route	*prev;
	u32		 ct_prio;	/*Priority of the route rule.*/
	u_char		 ct_table;	/*The route table*/
	u32		 ct_dst;	/*The connection's dst. The ip is converted with iptomap*/
	u32		 ct_gw;		/*Gateway*/
};
typedef struct ctable_route ct_route;

/* Connection Table entry. This keeps all the connection infos*/
struct ct_entry
{
	u32 		ct_conn; 	/*Total connections*/
	ct_route	*ctr;
};

struct rnode_rtable
{
	struct rnode_rtable *next;
	struct rnode_rtable *prev;
	u32		 *rt_rnode;	/*This points to the rnode's struct in the int_map*/
	struct ct_entry   rt_ct;
};
typedef struct rnode_rtable rnode_rt;



void ctr_add(ct_entry *ct, ct_route *ctr);
u_char rt_find_table(ct_route *ctr, u32 dst, u32 gw)
void krnl_update_node(map_node *node);
void rt_update(void);
int rt_add_def_gw(char *dev);
