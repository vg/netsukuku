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
 * igs.c:
 * Internet Gateway Search
 */

#include "includes.h"

#include "llist.c"
#include "igs.h"
#include "xmalloc.h"
#include "log.h"


/*
 * bandwidth_in_8bit:
 * `x' is the bandwidth value expressed in Kb/s.
 * 
 * Since we consider `x' expressed in this form:
 * 	 x = y * 2^y; 
 * we can store just `y' in a u_char (8bit) variable.
 *
 * `bandwidth_in_8bit' returns `y' from `x'.
 *
 * `x' cannot be greater than 3623878656 (27*2^27), so if `x' is in Kb/s the
 * maximum bandwidth we can store in a byte is 3.6Tb/s.
 */
u_char bandwidth_in_8bit(u_int x)
{
	u_int i,z,a,b;
	u_int diff_2;

	for(z=27;z>=0;z--) {
		
		i=z<<z;
		if(i==x)
			/* x is exactly z*2^z */
			return (u_char)z;
	
		b=(z-1)<<(z-1);
		diff_2=(i-b)>>1;
		if(x >= i-diff_2 && x <=i)
			/* `x' is nearer to z*2^z than (z-1)*2^(z-1) */ 
			return z;

		a = z == 27 ? i : (z+1)<<(z+1);
		diff_2=(a-i)>>1;
		if(x <= i+diff_2 && x >= i)
			/* `x' is nearer to z*2^z than (z+1)*2^(z+1) */ 
			return z;
	}
	return 0;
}

void init_igws(inet_gw ***igws, int **igws_counter, int levels)
{
	*igws=xmalloc(sizeof(inet_gw *) * levels);
	*igws_counter=(int *)xmalloc(sizeof(int)*levels);
	
	memset(*igws, 0, sizeof(inet_gw *) * levels);
	memset(*igws_counter, 0, sizeof(int)*levels);
}

void free_igws(inet_gw **igws, int *igws_counter, int levels)
{
	int i;
	
	for(i=0; i<levels; i++) {
		list_destroy(igws[i]);
		igws_counter[i]=0;
	}

	xfree(igws);
	xfree(igws_counter);
}

/*
 * igw_add_node: adds a new gw in the `igws[`level']' llist.
 */
void igw_add_node(inet_gw **igws, int *igws_counter,  int level,
		int gid, map_node *node, u_char bandwidth)
{
	inet_gw *igw;
	
	igw=xmalloc(sizeof(inet_gw));
	memset(igw, 0, sizeof(inet_gw));

	igw->node=node;
	igw->gid=gid;
	igw->bandwidth=bandwidth;
		
	clist_add(&igws[level], &igws_counter[level], igw);
}

/*
 * igw_find_node: finds an inet_gw struct in the `igws[`level']' llist which
 * has points to the given `node'. The pointer to the found struct is
 * returned, otherwise 0.
 */
inet_gw *igw_find_node(inet_gw **igws, int level, map_node *node)
{
	inet_gw *igw;

	igw=igws[level];
	list_for(igw)
		if(igw->node == node)
			return igw;
	return 0;
}

/*
 * igw_del_node: deletes, from the `igws[`level']' llist, the inet_gw struct
 * which points to `node'. On success 0 is returned.
 */
int igw_del_node(inet_gw **igws, int *igws_counter,  int level,
		map_node *node)
{
	inet_gw *igw;

	igw=igw_find_node(igws, level, node);
	if(!igw)
		return -1;

	clist_del(&igws[level], &igws_counter[level], igw);
	return 0;
}

int igw_bandwidth_cmp(const void *a, const void *b)
{
	inet_gw *gw_a=(inet_gw *)a;
	inet_gw *gw_b=(inet_gw *)b;
	
	if(gw_a->bandwidth > gw_b->bandwidth)
		return 1;
	else if(gw_a->bandwidth == gw_b->bandwidth)
		return 0;
	else
		return -1;
}

/*
 * igw_bandwidth_order: orders in decrescent order the `igws[`level']' llist,
 * comparing the igws[level]->bandwidth value.
 */
void igw_bandwidth_order(inet_gw **igws, int *igws_counter, int level)
{
	inet_gw *igw, *igw_tmp;
	int i;
		
	if(!igws_counter[level] || !igws[level])
		return;
	
	igw_tmp=xmalloc(sizeof(inet_gw)*igws_counter[level]);
	
	/*
	 * Save a copy of the igws[leve] llist in the `igw_tmp' static buffer
	 * to let `qsort' sort it ._^
	 */
	i=0;
	igw=igws[level];
	list_for(igw) {
		memcpy(&igw_tmp[i], igw, sizeof(inet_gw));
		i++;
	}

	qsort(igw_tmp, i, sizeof(inet_gw), igw_bandwidth_cmp);

	/* 
	 * Restore igws[level] 
	 */
	i=0;
	igw=igws[level];
	list_for(igw) {
		memcpy(igw, &igw_tmp[i], sizeof(inet_gw));
		i++;
	}

	xfree(igw_tmp);
}
