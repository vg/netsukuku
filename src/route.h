/* This file is part of Netsukuku system
 * (c) Copyright 2004 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#ifndef ROUTE_H
#define ROUTE_H


#define MAX_ROUTE_TABLES	253

/*The default number of levels that will be kept in the kernel route table*/
#define DEFAULT_ROUTE_LEVELS	3


/* * * Functions declaration * * */
void *get_gw_gnode(map_node *, map_gnode **, map_bnode **, 
		u_int *, map_gnode *, u_char, u_char);
int get_gw_ip(map_node *int_map, map_gnode **ext_map,
		map_bnode **bnode_map, u_int *bmap_nodes, 
		quadro_group *cur_quadg, map_gnode *find_gnode, 
		u_char gnode_level, u_char gw_level, inet_prefix *gw_ip);
void krnl_update_node(inet_prefix *dst_ip, void *dst_node, quadro_group *dst_quadg, 
		      void *void_gw, u_char level);
void rt_rnodes_update(int check_update_flag);
void rt_full_update(int check_update_flag);

int rt_add_gw(char *dev, inet_prefix to, inet_prefix gw);
int rt_del_gw(char *dev, inet_prefix to, inet_prefix gw);
int rt_change_gw(char *dev, inet_prefix to, inet_prefix gw);
int rt_replace_gw(char *dev, inet_prefix to, inet_prefix gw);
int rt_replace_def_gw(char *dev, inet_prefix gw);

int rt_del_loopback_net(void);

#endif /*ROUTE_H*/
