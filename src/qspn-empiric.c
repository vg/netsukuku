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

#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>

#include "qspn-empiric.h"
#include "map.h"
#include "xmalloc.h"

/* gen_rnd_map: Generate Random Map*/
void gen_rnd_map(int start_node) 
{
	int x, i=start_node, r, e, rnode_rnd, ms_rnd;
	map_rnode rtmp;
	
	if(i > MAXGROUPNODE)
		i=(rand()%(MAXGROUPNODE-0+1))+0;
	
	for(x=0; x<MAXGROUPNODE; x++) {
		if(int_map[rnode_rnd] & MAP_SNODE)
			continue;

		r=(rand()%(MAXLINKS-0+1))+0;          /*rnd_range algo: (rand()%(max-min+1))+min*/
		int_map[i].flags|=MAP_SNODE;
		for(e=0; e<=r; e++) {
			memset(&rtmp, '\0', sizeof(map_node));
			rnode_rnd=(rand()%(MAXGROUPNODE-0+1))+0;
			rtmp.r_node=(rnode_rnd*sizeof(map_node))+int_map;
			ms_rnd=(rand()%((MAXRTT*1000)-0+1))+0;
			rtmp.rtt.tv_usec=ms_rnd*1000;
			rnode_add(int_map[i], &rtmp);

			if(int_map[rnode_rnd] & ~MAP_SNODE)
				gen_rnd_map(rnode_rnd);
		}
			
		
	}
}

void *send_qspn_backpro(struct q_opt qopt)
{
	struct q_opt *qopt=argv, *nopt;
	int x, dst, pkt, to=qopt->q.to;
	pthread_t thread;

	usleep(qopt->sleep);

	/*Now we store the received pkt in our pkt_db*/
	nopt=xmalloc(sizeof(struct q_opt));

	pthread_mutex_lock(&mutex[to]);	
	pkt=pkt_dbc[to];
	pkt_dbc[to]++;
	pthread_mutex_unlock(&mutex[to]);	

	pkt_db[to][pkt]=xmalloc(sizeof(struct q_pkt));
	pkt_db[to][pkt].routes=qopt->q.routes++;
	if(pkt_db[to][pkt].routes) {
		pkt_db[to][pkt].tracer=xmalloc(sizeof(short)*pkt_db[to][pkt].routes);
		for(x=0; x<qopt->q.routes; x++)
			pkt_db[to][pkt].tracer[x]=qopt->q.tracer[x];
		/*Let's add our entry in the tracer pkt*/
		pkt_db[to][pkt].tracer[x][x+1]=to;
	}
	pkt_db[to][pkt].op=qopt->q.op;
	pkt_db[to][pkt].broadcast=qopt.q.broadcast;


	/*We've arrived... finally*/
	if(int_map[to].flags & QSPN_STARTER)
		return;

	nopt=xmalloc(sizeof(struct q_opt));

	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		if(int_map[to].r_node[x].flags & QSPN_CLOSED) {
			dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);

			gbl_stat.total_pkts++;
			node_stat[to].total_pkts++;

			memset(&nopt, 0, sizeof(struct q_opt));
			nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
			nopt->q.to=dst;
			nopt->q.from=to;
			nopt->q.routes=pkt_db[to][pkt].routes;
			nopt->q.broadcast=pkt_db[to][pkt].broadcast;

			gbl_stat.qspn_backpro++;
			node_stat[r].qspn_backpro++;
			nopt->q.op=OP_BACKPRO;
			pthread_create(&thread, NULL, send_qspn_backpro, (void *)nopt);
		}

	}
	xfree(nopt);
}

void *send_qspn_reply(void *argv)
{
	struct q_opt *qopt=argv, *nopt;
	int x, dst, pkt, to=qopt->q.to;
	pthread_t thread;

	usleep(qopt->sleep);

	/*Bad old broadcast pkt*/
	if(qopt.q.broadcast <= int_map[from].broadcast[to])
		return;

	/*Now we store the received pkt in our pkt_db*/
	nopt=xmalloc(sizeof(struct q_opt));

	pthread_mutex_lock(&mutex[to]);	
	pkt=pkt_dbc[to];
	pkt_dbc[to]++;
	pthread_mutex_unlock(&mutex[to]);	

	pkt_db[to][pkt]=xmalloc(sizeof(struct q_pkt));
	pkt_db[to][pkt].routes=qopt->q.routes++;
	if(pkt_db[to][pkt].routes) {
		pkt_db[to][pkt].tracer=xmalloc(sizeof(short)*pkt_db[to][pkt].routes);
		for(x=0; x<qopt->q.routes; x++)
			pkt_db[to][pkt].tracer[x]=qopt->q.tracer[x];
		/*Let's add our entry in the tracer pkt*/
		pkt_db[to][pkt].tracer[x][x+1]=to;
	}
	pkt_db[to][pkt].op=qopt->q.op;
	pkt_db[to][pkt].broadcast=qopt.q.broadcast;
	
	/*Let's keep broadcasting*/
	nopt=xmalloc(sizeof(struct q_opt));
	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);

		gbl_stat.total_pkts++;
		node_stat[to].total_pkts++;

		memset(&nopt, 0, sizeof(struct q_opt));
		nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
		nopt->q.to=dst;
		nopt->q.from=to;
		nopt->q.routes=pkt_db[to][pkt].routes;
		nopt->q.broadcast=pkt_db[to][pkt].broadcast;

		gbl_stat.qspn_replies++;
		node_stat[r].qspn_replies++;
		nopt->q.op=OP_REPLY;
		pthread_create(&thread, NULL, send_qspn_reply, (void *)nopt);
	}
	xfree(nopt);
}

void *send_qspn_pkt(void *argv)
{
	struct q_opt *qopt=argv;
	pthread_t thread;
	int x, i=0, dst, pkt, to=qopt->q.to;
	struct q_opt *nopt;

	usleep(qopt->sleep);

	nopt=xmalloc(sizeof(struct q_opt));
	
	pthread_mutex_lock(&mutex[to]);	
	pkt=pkt_dbc[to];
	pkt_dbc[to]++;
	pthread_mutex_unlock(&mutex[to]);	
	
	pkt_db[to][pkt]=xmalloc(sizeof(struct q_pkt));
	pkt_db[to][pkt].routes=qopt->q.routes++;
	if(pkt_db[to][pkt].routes) {
		pkt_db[to][pkt].tracer=xmalloc(sizeof(short)*pkt_db[to][pkt].routes);
		for(x=0; x<qopt->q.routes; x++)
			pkt_db[to][pkt].tracer[x]=qopt->q.tracer[x];
		/*Let's add our entry in the tracer pkt*/
		pkt_db[to][pkt].tracer[x][x+1]=to;
	}
	pkt_db[to][pkt].op=qopt->q.op;
	pkt_db[to][pkt].broadcast=qopt.q.broadcast;
	
	for(x=0; x<int_map[to].links; x++) {
		if(int_map[to].r_node[x].r_node == from) {
			int_map[to].r_node[x].flags|=QSPN_CLOSED;
			break;
		}
		if(int_map[to].r_node[x].flags & ~QSPN_CLOSED)
			i++;
	}
	if(!i) {
		/*W00t I'm an extreme node!*/
		gbl_stat.total_pkts++;
		node_stat[to].total_pkts++;
		
		memset(&nopt, 0, sizeof(struct q_opt));
		nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
		nopt->q.to=dst;
		nopt->q.from=to;
		nopt->q.routes=pkt_db[to][pkt].routes;
		memcpy(nopt->q.tracer, pkt_db[to][pkt].tracer, sizeof(short)*pkt_db[to][pkt].routes);
		nopt->q.op=OP_BACKPRO;
		nopt->q.broadcast=int_map[to].broadcast[to]++;
		
		gbl_stat.qspn_replies++;
		node_stat[to].qspn_replies++;
		xfree(qopt);
		send_qspn_reply(nopt);
		return;
	}
	
	for(x=0; x<int_map[to].links; x++) {	
		if(int_map[to].r_node[x].r_node == from) 
			continue;

		dst=(int_map[to].r_node[x].r_node-int_map)/sizeof(map_node);
		gbl_stat.total_pkts++;
		node_stat[to].total_pkts++;

		memset(&nopt, 0, sizeof(struct q_opt));
		nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
		nopt->q.to=dst;
		nopt->q.from=to;
		nopt->q.broadcast=pkt_db[to][pkt].broadcast;
		nopt->q.routes=pkt_db[to][pkt].routes;
		memcpy(nopt->q.tracer, pkt_db[to][pkt].tracer, sizeof(short)*pkt_db[to][pkt].routes);

		if(int_map[qopt->q.to].r_node[x].flags & QSPN_CLOSED) {
			gbl_stat.qspn_backpro++;
			node_stat[to].qspn_backpro++;
			nopt->q.op=OP_REPLY;
			xfree(qopt);
			pthread_create(&thread, NULL, send_qspn_backpro, (void *)nopt);
		} else {
			gbl_stat.qspn_requests++;
			node_stat[to].qspn_requests++;
			nopt->q.op=OP_REQUEST;
			xfree(qopt);
			pthread_create(&thread, NULL, send_qspn_pkt, (void *)nopt);
		}
	}
}

void collect_data(void)
{
	int i, x, e;

	for(i=0; i<MAXGROUPNODE; i++)
		for(e=0; pkt_db[i][e].routes; e++)
			for(x=0; x<pkt_db[i][e].routes; x++)
				if((rt_stat[i][pkt_db[i][e].tracer[x]]++)==1)
					rt_total[i]++;
}

void print_data(char *file)
{
	int i, x, e;
	FILE *fd;

	fd=fopen((file), "a");

	fprintf(fd, "---- Test dump n. 6 ----\n");
	
	for(i=0; i<MAXGROUPNODE; i++)
		if(rt_total[i]<MAXGROUPNODE)
			fprintf(fd,"*WARNING* The node %d has only %d/%d routes\n *WARNING*\n", i, rt_total[i], MAXGROUPNODE);

	fprintf(fd, "Gbl_stat{\n\ttotal_pkts: %d\n\tqspn_requests: %d\n\t",
			"qspn_replies: %d\n\tqspn_backpro: %d }\n",
			gbl_stat.total_pkts, gbl_stat.qspn_requests,
			gbl_stat.qspn_replies, gbl_stat.qspn_backpro);
	
	for(i=0; i<MAXGROUPNODE; i++) {	
		fprintf(fd, "Total routes for %d node: ");
		for(x=0; MAXGROUPNODE; x++)
			fprintf(fd, "(%d)%d ", x, rt_stat[i][x]);
	}
		
	fprintf("\n--\n\n");
	fprintf("Node single stats\n");

	for(i=0; i<MAXGROUPNODE; i++)
		fprintf(fd, "%d_stat{\n\ttotal_pkts: %d\n\tqspn_requests: %d\n\t",
			"qspn_replies: %d\n\tqspn_backpro: %d }\n", i,
			node_stat[i].total_pkts, node_stat[i].qspn_requests,
			node_stat[i].qspn_replies, node_stat[i].qspn_backpro);

	fprintf(fd, "Pkts dump\n");
	for(i=0; i<MAXGROUPNODE; i++) {
		for(x=0; x<pkt_dbc[i]; x++) {
			fprintf(fd, "(%d) { op: %d, from: %d, broadcast: %d }\n",
					i, pkt_db[i][x].op, pkt_db[i][x].from,
					pkt_db[i][x].broadcast);
			fprintf(fd, "tracer: \n");
			for(e=0; e<pkt_db[i][x].routes; e++)
				fprintf(fd, "%d -> ",pkt_db[i][x].tracer[e]);
			fprintf(fd, "\n");
					
		}
	}
}

void clear_all(void)
{
	memset(&gbl_stat, 0, sizeof(struct stat));
	memset(&node_stat, 0, sizeof(struct stat)*MAXGROUPNODE);
	memset(&pkt_db, 0, sizeof(struct q_pkt)*MAXGROUPNODE);
	memset(&pkt_dbc, 0, sizeof(struct q_pkt)*MAXGROUPNODE);
	memset(&rt_stat, 0, sizeof(short)*MAXGROUPNODE*MAXGROUPNODE);
	memset(&rt_total, 0, sizeof(short)*MAXGROUPNODE);

}

int main(int argc, char **argv)
{
	struct q_opt *nopt;
	pthread_t thread;
	int i, r, e;
	
	
	clear_all();
	
	for(i=0; i<MAXGROUPNODE; i++) 
		pthread_mutex_init(&mutex[i], NULL);

	int_map=init_map(0);
	
	printf("Generating a random map...\n");
	i=(rand()%(MAXGROUPNODE-0+1))+0;
	gen_rnd_map(i);
	int_map[i].flags|=MAP_ME;
	
	printf("Running the first test...\n");
	r=(rand()%(MAXGROUPNODE-0+1))+0;
	printf("Starting the QSPN spreading from node %d\n", r);
	int_map[r].flags|=QSPN_STARTER;
	nopt=xmalloc(sizeof(struct q_opt));
	for(x=0; x<int_map[r].links; x++) {
		gbl_stat.total_pkts++;
		node_stat[r].total_pkts++;

		memset(&nopt, 0, sizeof(struct q_opt));
		nopt->sleep=int_map[r].r_node[x].rtt.tv_usec;
		nopt->q.to=x;
		nopt->q.from=r;
		nopt->q.routes=0;
		nopt->q.broadcast=0;

		gbl_stat.qspn_requests++;
		node_stat[r].qspn_requests++;
		nopt->q.op=OP_REQUEST;
		pthread_create(&thread, NULL, send_qspn_pkt, (void *)nopt);
	}
	pthread_join(thread, NULL);	
	xfree(nopt);
	int_map[r].flags&=~QSPN_STARTER;
	
	printf("Saving the data to QSPN-test-1 and clearing");
	collect_data();
	print_data("QSPN1");
	for(x=0; x<MAXGROUPNODE; x++) {
		for(e=0; pkt_db[to][e].routes; e++) {
			xfree(pkt_db[to][d].tracer);
			xfree(pkt_db[to][d]);
		}
	}
	clear_all();
	exit(0);
}
