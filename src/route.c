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

#include "includes.h"

#include "llist.c"
#include "libnetlink.h"
#include "inet.h"
#include "krnl_route.h"
#include "pkts.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "netsukuku.h"
#include "route.h"
#include "xmalloc.h"
#include "log.h"

u_char rt_find_table(ct_route *ctr, u_int dst, u_int gw)
{
	ct_route *i;
	u_char tables[MAX_ROUTE_TABLES];
	int l;
	
	memset(tables, '\0', MAX_ROUTE_TABLES);
	
	for(i=ctr; i; i=i->next) {
		if(i->ct_dst==dst) {
			if(i->ct_gw==gw)
				return i->ct_table;
			else 
				tables[i->ct_table]=1;
		}
	}

	for(l=1; l<MAX_ROUTE_TABLES; l++)
		if(!tables[l])
			return l;

	return 0xff; /*This shouldn't happen!*/
}

void krnl_update_node(void *void_node, u_char level)
{
	map_node *node, *gw_node;
	map_gnode *gnode;
	map_gnode *gto;	/*Great Teacher Onizuka*/
	struct nexthop *nh;
	inet_prefix to;
	int i, node_pos;

	nh=0;
	node=(map_node *)void_node;
	gnode=(map_gnode *)void_node;

	if(!level) {
		nh=xmalloc(sizeof(struct nexthop)*(node->links+1));
		memset(nh, '\0', sizeof(struct nexthop)*(node->links+1));

		maptoip((u_int)me.int_map, (u_int)node, me.cur_quadg.ipstart[1], &to);

		for(i=0; i<node->links; i++) {
#ifdef QMAP_STYLE_I
			maptoip((u_int)me.int_map, (u_int)get_gw_node(node, i),
					me.cur_quadg.ipstart[1], &nh[i].gw);
#else /*QMAP_STYLE_II*/
			maptoip((u_int)me.int_map, (u_int)node->r_node[i].r_node,
					me.cur_quadg.ipstart[1], &nh[i].gw);
#endif
		inet_htonl(&nh[i].gw);
			nh[i].dev=me.cur_dev;
			nh[i].hops=255-i;
		}
		nh[node->links].dev=0;
		node_pos=pos_from_node(node, me.int_map);
	} else {
		nh=xmalloc(sizeof(struct nexthop)*2);
		memset(nh, '\0', sizeof(struct nexthop)*2);
		
		node=&gnode->g;
		node_pos=pos_from_gnode(gnode, me.ext_map[_EL(level)]);
		gnodetoip(me.ext_map, &me.cur_quadg, gnode, level, &to);
		inet_htonl(&to);
		
		gw_node=get_gw_gnode(me.int_map, me.ext_map, me.bnode_map,
				me.bmap_nodes, gnode, level, 0);
		maptoip((u_int)me.int_map, (u_int)gw_node, 
				me.cur_quadg.ipstart[1], &nh[0].gw);
		inet_htonl(&nh[0].gw);
		nh[0].dev=me.cur_dev;

		nh[1].dev=0;
	}
	
	if(node->flags & MAP_VOID) {
		/*Ok, let's delete it*/
		if(route_del(to, nh, me.cur_dev, 0))
			error("WARNING: Cannot delete the route entry for the ",
					"%d %cnode!", node_pos, !level ? ' ' : 'g');
	} else
		if(route_replace(to, nh, me.cur_dev, 0))
			error("WARNING: Cannot update the route entry for the "
					"%d %cnode!", node_pos, !level ? ' ' : 'g');
	if(nh)
		xfree(nh);
}

void rt_update(void)
{
	u_short i, l;
	
	for(l=0; l<me.cur_quadg.levels; l++)
		for(i=0; i<MAXGROUPNODE; i++) {
			if(me.int_map[i].flags & MAP_UPDATE && !(me.int_map[i].flags & MAP_RNODE)) {
				krnl_update_node(&me.int_map[i], l);
				me.int_map[i].flags&=~MAP_UPDATE;
			}
		}
	route_flush_cache(my_family);
}

int rt_exec_gw(char *dev, inet_prefix to, inet_prefix gw, 
		int (*route_function)(inet_prefix to, struct nexthop *nhops, char *dev, u_char table) )
{	struct nexthop nh[2];
	
	inet_htonl(&to);

	memset(nh, '\0', sizeof(struct nexthop)*2);	
	memcpy(&nh[0].gw, &gw, sizeof(inet_prefix));
	inet_htonl(&nh[0].gw);
	nh[0].dev=dev;
	nh[1].dev=0;

	return route_function(to, nh, dev, 0);
}

int rt_add_gw(char *dev, inet_prefix to, inet_prefix gw)
{
	return rt_exec_gw(dev, to, gw, route_add);
}

int rt_del_gw(char *dev, inet_prefix to, inet_prefix gw)
{
	return rt_exec_gw(dev, to, gw, route_del);
}

int rt_change_gw(char *dev, inet_prefix to, inet_prefix gw)
{
	return rt_exec_gw(dev, to, gw, route_change);
}

int rt_replace_gw(char *dev, inet_prefix to, inet_prefix gw)
{
	return rt_exec_gw(dev, to, gw, route_replace);
}

int rt_replace_def_gw(char *dev, inet_prefix gw)
{
	struct nexthop nh[2];
	inet_prefix to;
	
	if(inet_setip_anyaddr(&to, my_family)) {
		error("rt_add_def_gw(): Cannot use INADRR_ANY for the %d family\n", to.family);
		return -1;
	}
	to.len=0;
	to.bits=0;

	return rt_replace_gw(dev, to, gw);
}
