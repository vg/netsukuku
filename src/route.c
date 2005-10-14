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
 *
 * --
 * route.c:
 * Routing table management code.
 */

#include "includes.h"

#include "misc.h"
#include "llist.c"
#include "libnetlink.h"
#include "inet.h"
#include "krnl_route.h"
#include "request.h"
#include "endianness.h"
#include "pkts.h"
#include "bmap.h"
#include "qspn.h"
#include "netsukuku.h"
#include "route.h"
#include "xmalloc.h"
#include "log.h"


/* 
 * get_gw_gnode: It returns the pointer to the gateway node present in the map
 * of level `gw_level'. This gateway is the node to be used as gateway to
 * reach, from the `gw_level' level,  the `find_gnode' gnode at the `gnode_level'
 * level. If the gw_node isn't found, NULL is returned.
 */
void *get_gw_gnode(map_node *int_map, map_gnode **ext_map,
		map_bnode **bnode_map, u_int *bmap_nodes, 
		map_gnode *find_gnode, u_char gnode_level, 
		u_char gw_level)
{
	map_gnode *gnode;
	map_gnode *gnode_gw;
	map_node  *node, *node_gw, *root_node;
	ext_rnode_cache *erc;
	int i, pos, bpos;

	if(!gnode_level || gw_level > gnode_level)
		return 0;

	/* 
	 * How it works:
	 * - Start from the level `gnode_level' and the gnode `find_gnode', set
	 *   `node' to `find_gnode'.
	 * loop:
	 * 	- Use the map at the current level to get the gw to reach
	 * 	  `node', which it is the current gnode (at the current level). 
	 * 	  If `node' is a rnode set the gw to `node' itself, if instead 
	 * 	  it is a MAP_ME gnode, just set gw = `find_gnode'.
	 * 	- If the level is 0, all is done, return the gw.
	 * 	- Go one level down: level--;
	 * 	- At this level use the bnode map and look for the bnode which
	 * 	  borderes on the gw of the upper level we found. (Note that all
	 * 	  the bnodes in the bmap always point at the upper level).
	 * 	- Find the gw to reach the found bnode at this level and set 
	 * 	  `node' to this gw.
	 * 	- goto loop;
	 */
	gnode=find_gnode;
	node=&gnode->g;

	/* The gateway to reach me is myself. */
	if(node->flags & MAP_ME) 
		return (void *)node;

	debug(DBG_INSANE, "get_gw: find_gnode=%x", find_gnode); 
	for(i=gnode_level; i>=gw_level; i--) {

		if(node->flags & MAP_RNODE) {
			gnode_gw=(void *)node;
			node_gw=(map_node *)gnode_gw;
			debug(DBG_INSANE, "get_gw: l=%d, node & MAP_RNODE. node_gw=node=%x",
			  i, node);
		} else if (node->flags & MAP_ME) {
			gnode_gw=(void *)find_gnode;
			node_gw=(map_node *)gnode_gw;
			debug(DBG_INSANE, "get_gw: l=%d, node & MAP_ME. find_gnode: %x",
			  i, find_gnode);
		} else {
			if(!node->links || (i && !gnode))
				return 0;

			pos=rand_range(0, node->links-1);
			if(!i) {
				node_gw=(void *)node->r_node[pos].r_node;
			} else {
				gnode_gw=(map_gnode *)gnode->g.r_node[pos].r_node;
				node_gw=(void *)gnode_gw;
			}
			if(node_gw->flags & MAP_RNODE)
				find_gnode=(map_gnode *)node_gw;
			debug(DBG_INSANE, "get_gw: l=%d, node_gw=rnode[pos].r_node=%x,"
			  " find_gnode=%x", i,node_gw, find_gnode);
		}

		if(i == gw_level)
			return (void *)node_gw;
		else if(!i)
			return 0;

		bpos=map_find_bnode_rnode(bnode_map[i-1], bmap_nodes[i-1], (void *)node_gw);
		if(bpos == -1) {
			debug(DBG_INSANE, "get_gw: l=%d, node_gw=%x not found in bmap lvl %d", 
			 i, node_gw, i-1);
			return 0;
		}

		if(!(i-1))
			node=node_from_pos(bnode_map[i-1][bpos].bnode_ptr, int_map);
		else {
			gnode=gnode_from_pos(bnode_map[i-1][bpos].bnode_ptr, 
					ext_map[_EL(i-1)]);
			node=&gnode->g;

			qspn_set_map_vars(i-1, 0, &root_node, 0, 0);
			if(me.cur_node->flags & MAP_BNODE && 
					gnode == (map_gnode *)root_node) {
				debug(DBG_INSANE, "get_gw: bmap searching ernode for gnode 0x%x",node_gw);
		
				erc=erc_find_gnode(me.cur_erc, gnode_gw, i);
				if(erc)
					return (void *)erc->e;
			}
		}
		debug(DBG_INSANE, "get_gw: bmap found = %x", node);
	}

	return 0;
}

/*
 * get_gw_ip: It's a wrapper to get_gw_gnode() that stores directly the gw's
 * ip in `gw_ip'.
 * On error -1 is returned.
 */
int get_gw_ip(map_node *int_map, map_gnode **ext_map,
		map_bnode **bnode_map, u_int *bmap_nodes, 
		quadro_group *cur_quadg,
		map_gnode *find_gnode, u_char gnode_level, 
		u_char gw_level, inet_prefix *gw_ip)
{
	ext_rnode *e_rnode=0;
	map_node *gw_node=0;

	gw_node=get_gw_gnode(int_map, ext_map, bnode_map, bmap_nodes, 
			find_gnode, gnode_level, gw_level);
	
	if(!gw_node)
		return -1;

	if(gw_node->flags & MAP_ERNODE) {
		e_rnode=(ext_rnode *)gw_node;
		memcpy(gw_ip, &e_rnode->quadg.ipstart[gw_level], sizeof(inet_prefix));
	} else
		maptoip((u_int)int_map, (u_int)gw_node, cur_quadg->ipstart[1], 
				gw_ip);

	return 0;
}


/* 
 * krnl_update_node: It adds/replaces or removes a route from the kernel's
 * table, if the node's flag is found, respectively, to be set to 
 * MAP_UPDATE or set to MAP_VOID. The destination of the route can be given
 * with `dst_ip', `dst_node' or `dst_quadg'.
 * If `dst_ip' is not null, the given inet_prefix struct is used, it's also 
 * used the `dst_node' to retrieve the flags.
 * If the destination of the route is a node which belongs to the level 0, it
 * must be passed to `dst_node'. 
 * If `level' is > 0 and `dst_quadg' is not null, then it updates the gnode
 * which is inside the `dst_quadg' struct: dst_quadg->gnode[_EL(level)]. The 
 * quadro_group struct must be complete and refer to the groups of the 
 * given gnode. 
 * If `level' is > 0 and `dst_quadg' is null, it's assumed that the gnode is passed
 * in `dst_node' and that the quadro_group for that gnode is me.cur_dst_quadg.
 * If `void_gw' is not null, it is used as the only gw to reach the destination 
 * node, otherwise the gw will be calculated.
 */
void krnl_update_node(inet_prefix *dst_ip, void *dst_node, quadro_group *dst_quadg, 
		      void *void_gw, u_char level)
{
	ext_rnode *e_rnode=0;
	map_node *node=0, *gw_node=0;
	map_gnode *gnode=0;
	struct nexthop *nh=0;
	inet_prefix to;
	int i, node_pos=0, route_scope=0, err;
#ifdef DEBUG		
	char *to_ip, *gw_ip;
#endif

	node=(map_node *)dst_node;
	gnode=(map_gnode *)dst_node;

	/* 
	 * Deduce the destination's ip 
	 */
	if(dst_ip)
		memcpy(&to, dst_ip, sizeof(inet_prefix));
	else if(level) {
		if(!dst_quadg) {
			dst_quadg=&me.cur_quadg;
			node_pos=pos_from_gnode(gnode, me.ext_map[_EL(level)]);
		} else {
			gnode=dst_quadg->gnode[_EL(level)];
			node_pos=dst_quadg->gid[level];
		}
		node=&gnode->g;
		gnodetoip(dst_quadg, node_pos, level, &to);
	} else {
		node_pos=pos_from_node(node, me.int_map);
		maptoip((u_int)me.int_map, (u_int)node, me.cur_quadg.ipstart[1], &to);
	}
#ifdef DEBUG		
	to_ip=xstrdup(inet_to_str(to));
#endif
	inet_htonl(to.data, to.family);

	if(void_gw)
		gw_node=(map_node *)void_gw;

	/* 
	 * If `node' it's a rnode of level 0, do nothing! It is already 
	 * directly connected to me. (If void_gw is not null, skip this check).
	 */
	if(node->flags & MAP_RNODE && !level && !void_gw)
		goto finish;
	
	if(node->flags & MAP_ME)
		goto finish;

	/*
	 * Now, get the gateway to reach the destination.
	 */
	if(node->flags & MAP_VOID) {
		goto do_update;
		
	} else if(void_gw) {
		nh=xmalloc(sizeof(struct nexthop)*2);
		memset(nh, '\0', sizeof(struct nexthop)*2);
		
		if(gw_node->flags & MAP_ERNODE) {
			e_rnode=(ext_rnode *)gw_node;
			memcpy(&nh[0].gw, &e_rnode->quadg.ipstart[0], sizeof(inet_prefix));
		} else 
			maptoip((u_int)me.int_map, (u_int)gw_node, 
					me.cur_quadg.ipstart[1], &nh[0].gw);
#ifdef DEBUG		
		gw_ip=xstrdup(inet_to_str(nh[0].gw));
#endif
		inet_htonl(nh[0].gw.data, nh[0].gw.family);
		nh[0].dev=me.cur_dev;
		nh[1].dev=0;
	} else if(!level) {
		nh=xmalloc(sizeof(struct nexthop)*(node->links+1));
		memset(nh, '\0', sizeof(struct nexthop)*(node->links+1));
		
		if(!(node->flags & MAP_VOID))
			for(i=0; i<node->links; i++) {
				maptoip((u_int)me.int_map, (u_int)node->r_node[i].r_node,
						me.cur_quadg.ipstart[1], &nh[i].gw);
#ifdef DEBUG		
				if(!i)
					gw_ip=xstrdup(inet_to_str(nh[0].gw));
#endif
				inet_htonl(nh[i].gw.data, nh[i].gw.family);
			}
		nh[i].dev=me.cur_dev;
		nh[i].hops=255-i;
		nh[node->links].dev=0;
	} else if(level) {
		/* TODO: Support for the gnode multipath using nexthop */
		nh=xmalloc(sizeof(struct nexthop)*2);
		memset(nh, '\0', sizeof(struct nexthop)*2);

		err=get_gw_ip(me.int_map, me.ext_map, me.bnode_map,
			     me.bmap_nodes, &me.cur_quadg,
			     gnode, level, 0, &nh[0].gw);
		if(err < 0) {
#ifdef DEBUG
			debug(DBG_NORMAL, "Cannot get the gateway for "
					"the gnode: %d of level: %d, ip:"
					"%s", node_pos, level, to_ip);
#endif
			goto finish;
		}
#ifdef DEBUG
		gw_ip=xstrdup(inet_to_str(nh[0].gw));
#endif
		inet_htonl(nh[0].gw.data, nh[0].gw.family);
		nh[0].dev=me.cur_dev;
		nh[1].dev=0;
	}

do_update:
#ifdef DEBUG
	if(node->flags & MAP_VOID)
		gw_ip=to_ip;
	debug(DBG_INSANE, "krnl_update_node: to %s/%d via %s", to_ip, to.bits ,gw_ip);
	xfree(to_ip);
	if(!(node->flags & MAP_VOID))
	xfree(gw_ip);
#endif
	if(node->flags & MAP_RNODE && !level)
		route_scope = RT_SCOPE_LINK;

	if(node->flags & MAP_VOID) {
		/*Ok, let's delete it*/
#ifndef DEBUG
		if(route_del(RTN_UNICAST, 0, to, 0, me.cur_dev, 0))
			error("WARNING: Cannot delete the route entry for the ",
					"%cnode %d lvl %d!", !level ? ' ' : 'g',
					node_pos, level);
#else
	#warning ***The route_del code is disabled***
#endif
	} else if(route_replace(0, route_scope, to, nh, me.cur_dev, 0))
			error("WARNING: Cannot update the route entry for the "
					"%cnode %d lvl %d",!level ? ' ' : 'g',
					node_pos, level);
finish:
	if(nh)
		xfree(nh);
}

/* 
 * rt_rnodes_update: It updates all the node which are rnodes of the root_node
 * of all the maps. If `check_update_flag' is non zero, the rnode will be
 * updated only if it has the MAP_UPDATE flag set.
 */
void rt_rnodes_update(int check_update_flag)
{
	u_short i, level;
	ext_rnode *e_rnode;
	map_node *root_node, *node, *rnode;
	map_gnode *gnode;

	/* If we aren't a bnode it's useless to do all this */
	if(!(me.cur_node->flags & MAP_BNODE))
		return;
	
	/* Internal map */
	root_node=me.cur_node;
	for(i=0; i < root_node->links; i++) {
		rnode=(map_node *)root_node->r_node[i].r_node;

		if(rnode->flags & MAP_ERNODE) {
			level=0;
			e_rnode=(ext_rnode *)rnode;
			
			if(!check_update_flag || rnode->flags & MAP_UPDATE) {
				krnl_update_node(&e_rnode->quadg.ipstart[0], rnode, 0,
						me.cur_node, level);
				rnode->flags&=~MAP_UPDATE;
			}

			for(level=1; level < e_rnode->quadg.levels; level++) {
				gnode = e_rnode->quadg.gnode[_EL(level)];
				if(!gnode)
					continue;

				node = &gnode->g;
				if(!check_update_flag || node->flags & MAP_UPDATE) {
					krnl_update_node(0, 0, &e_rnode->quadg,
							rnode, level);
					node->flags&=~MAP_UPDATE;
				}
			}
		}
	}
}

/* 
 * rt_full_update: It updates _ALL_ the possible routes it can get from _ALL_
 * the maps. If `check_update_flag' is not 0, it will update only the routes of
 * the nodes with the MAP_UPDATE flag set. Note that the MAP_VOID nodes aren't
 * considered.
 */
void rt_full_update(int check_update_flag)
{
	u_short i, l;

	/* Update ext_maps */
	for(l=me.cur_quadg.levels-1; l>=1; l--)
		for(i=0; i<MAXGROUPNODE; i++) {
			if(me.ext_map[_EL(l)][i].g.flags & MAP_VOID || 
				me.ext_map[_EL(l)][i].flags & GMAP_VOID ||
				me.ext_map[_EL(l)][i].g.flags & MAP_ME)
				continue;

			if(check_update_flag && 
				!(me.ext_map[_EL(l)][i].g.flags & MAP_UPDATE))
				continue;

			krnl_update_node(0, &me.ext_map[_EL(l)][i].g, 0, 0, l);
			me.ext_map[_EL(l)][i].g.flags&=~MAP_UPDATE;
		}

	/* Update int_map */
	for(i=0, l=0; i<MAXGROUPNODE; i++) {
		if(me.int_map[i].flags & MAP_VOID || me.int_map[i].flags & MAP_ME)
			continue;

		if(check_update_flag && !((me.int_map[i].flags & MAP_UPDATE)))
			continue;

		krnl_update_node(0, &me.int_map[i], 0, 0, l);
		me.int_map[i].flags&=~MAP_UPDATE;
	}

	route_flush_cache(my_family);
}

int rt_exec_gw(char *dev, inet_prefix to, inet_prefix gw, 
		int (*route_function)(ROUTE_CMD_VARS))
{
	struct nexthop nh[2], *neho;

	if(to.len)
		inet_htonl(to.data, to.family);

	if(gw.len) {
		memset(nh, '\0', sizeof(struct nexthop)*2);	
		memcpy(&nh[0].gw, &gw, sizeof(inet_prefix));
		inet_htonl(nh[0].gw.data, nh[0].gw.family);
		nh[0].dev=dev;
		nh[1].dev=0;
		neho=nh;
	} else
		neho=0;

	return route_function(0, 0, to, neho, dev, 0);
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
	inet_prefix to;

	if(inet_setip_anyaddr(&to, my_family)) {
		error("rt_add_def_gw(): Cannot use INADRR_ANY for the %d family", to.family);
		return -1;
	}
	to.len=0;
	to.bits=0;

	return rt_replace_gw(dev, to, gw);
}

/* 
 * rt_del_loopback_net:
 * We remove the loopback net, leaving only the 127.0.0.1 ip for loopback.
 *  ip route del local 127.0.0.0/8  proto kernel  scope host src 127.0.0.1
 *  ip route del broadcast 127.255.255.255  proto kernel scope link  src 127.0.0.1
 *  ip route del broadcast 127.0.0.0  proto kernel  scope link src 127.0.0.1
 */
int rt_del_loopback_net(void)
{
	inet_prefix to;
	char lo_dev[]="lo";
	u_int idata[MAX_IP_INT];

	memset(idata, 0, MAX_IP_SZ);
	if(my_family!=AF_INET) 
		return 0;

	/*
	 * ip route del broadcast 127.0.0.0  proto kernel  scope link      \
	 * src 127.0.0.1
	 */
	idata[0]=LOOPBACK_NET;
	inet_setip(&to, idata, my_family);
	route_del(RTN_BROADCAST, 0, to, 0, 0, RT_TABLE_LOCAL);

	/*
	 * ip route del local 127.0.0.0/8  proto kernel  scope host 	   \
	 * src 127.0.0.1
	 */
	to.bits=8;
	route_del(RTN_LOCAL, 0, to, 0, lo_dev, RT_TABLE_LOCAL);

	/* 
	 * ip route del broadcast 127.255.255.255  proto kernel scope link \
	 * src 127.0.0.1 
	 */
	idata[0]=LOOPBACK_BCAST;
	inet_setip(&to, idata, my_family);
	route_del(RTN_BROADCAST, 0, to, 0, lo_dev, RT_TABLE_LOCAL);

	return 0;
}
