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

#include <unistd.h>

#include "map.h"
#include "xmalloc.h"

map_node *int_map;
int generated_pkts=0;

int *tracer_pkt[MAXGROUPNODES];
int routes[MAXGROUPNODES];

/* gen_rnd_map: Generate Random Map*/
void gen_rnd_map(int start_node) 
{
	int x, i=start_node, r, e, rnode_rnd, ms_rnd;
	map_rnode rtmp;
	
	if(i > MAXGROUPNODES)
		i=(rand()%(MAXGROUPNODES-0+1))+0;
	
	for(x=0; x<MAXGROUPNODES; x++) {
		if(int_map[rnode_rnd] & MAP_SNODE)
			continue;

		r=(rand()%(MAXLINKS-0+1))+0;          /*rnd_range algo: (rand()%(max-min+1))+min*/
		int_map[i].flags|=MAP_SNODE;
		for(e=0; e<=r; e++) {
			memset(&rtmp, '\0', sizeof(map_node));
			rnode_rnd=(rand()%(MAXGROUPNODES-0+1))+0;
			rtmp.r_node=nr;
			ms_rnd=(rand()%((MAXRTT*1000)-0+1))+0;
			rtmp.rtt.tv_usec=ms_rnd*1000;
			map_rnode *rnode_add(int_map[i], &rtmp);

			if(int_map[rnode_rnd] & ~MAP_SNODE)
				gen_rnd_map(rnode_rnd);
		}
			
		
	}
}

void *send_qspn_backpro(int from, int to, int sleep, int rts)
{
	int x, dst;

	usleep(sleep);

	/*We've arrived... finally*/
	if(int_map[to].flags == MAP_ME)
		return;
	
	/*TODO: tracer here*/
	
	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		if(int_map[to].r_node[x].flags & QSPN_CLOSED) {
			dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);
			generated_pkts++;
			send_qspn_backpro(to, dst, int_map[to].r_node[x].rtt.tv_usec, rts);
		}

	}
}

void *send_qspn_reply(int from, int to, int sleep, int rts)
{
	int x, dst;

	usleep(sleep);

	/*We've arrived... finally*/
	if(int_map[to].flags == MAP_ME)
		return;

	/*TODO: tracer here*/

	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);
		generated_pkts++;
		send_qspn_reply(to, dst, int_map[to].r_node[x].rtt.tv_usec, rts);
	}
}

void *send_qspn_pkt(int from, int to, int sleep, int rts)
{
	int x, i=0, dst;

	usleep(sleep);
	
	routes[to]=rts++:
/*TODO: boh, memorizzare i tracer pkt in ogni nodo??
	tracer_pkt[to]=xrealloc(tracer_pkt[to], rts*sizeof(int));
	memcpy(tracer_pkt[to], tracer_pkt[from], routes*sizeof(int));
	new_tracer[routes-1]=to;
	*/
	
	for(x=0; x<int_map[to].links; x++) {
		if(int_map[to].r_node[x].r_node == from) {
			int_map[to].r_node[x].flags|=QSPN_CLOSED;
			break;
		}
		if(int_map[to].r_node[x].flags & ~QSPN_CLOSED)
			i++;
	}
	if(!i) {
		/*TODO: W00t I'm an extreme node!*/
	}
	
	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);
		generated_pkts++;

		if(int_map[to].r_node[x].flags & QSPN_CLOSED)
			send_qspn_backpro(to, dst, int_map[to].r_node[x].rtt.tv_usec, rts);

		send_qspn_pkt(to, dst, int_map[to].r_node[x].rtt.tv_usec, rts);
	}
}


int main()
{
	int i, r;
	
	memset(routes, 0, sizeof(int)*MAXGROUPNODES);
	int_map=init_map(0);
	i=(rand()%(MAXGROUPNODES-0+1))+0;
	gen_ran_map(i);

	r=(rand()%(MAXGROUPNODES-0+1))+0;
	int_map[r].flags|=MAP_ME;

	for(x=0; x<int_map[r].links; x++)
		send_qspn_pkt(r, x, int_map[r].r_node[x].rtt.tv_usec, 0);
}
