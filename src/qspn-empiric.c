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
 *
 * qspn-empiric:
 * This is the living proof of the QSPN algo. qspn-empiric simulates an
 * entire network and runs on it the QSPN algo. Then when all is done it
 * collects the generated data and makes some statistic, in this way I can 
 * tune without much worries the QSPN algorithm. Qspn-empiric can be also 
 * used to solve graph without using djkstra hehehe.
 * ah,..  yes it uses the threads... a lot of them... ^_^ I want a cluster!
 */

#include <stdlib.h>
#include <stdio.h>

#include "qspn-empiric.h"
#include "log.h"
#include "xmalloc.h"

/*thread_joint creates a thread in JOINED STATE or in DETACHED STATE*/
void thread_joint(int joint, void * (*start_routine)(void *), void *nopt)
{	
	pthread_t thread;
	total_threads++;
	if(joint) {
		fprintf(stderr, "%u: Joining the thread...", pthread_self());
		pthread_create(&thread, NULL, start_routine, (void *)nopt);
		fprintf(stderr, " %u\n", thread);
		pthread_join(thread, NULL);	
	} else {
		pthread_create(&thread, NULL, start_routine, (void *)nopt);
		pthread_detach(thread);
	}
}

/* gen_rnd_map: Generate Random Map*/
void gen_rnd_map(int start_node, int back_link, int back_link_rtt) 
{
	int i=start_node, r=0, e, b=0, rnode_rnd, ms_rnd;
	map_rnode rtmp;

	if(i > MAXGROUPNODE)
		i=(random()%(MAXGROUPNODE-0))+0;

	if(back_link>=0 && back_link<MAXGROUPNODE)
		b=1;
	
	if(int_map[i].flags & MAP_SNODE)
		return;
	
	r=(random()%(MAXLINKS-0+1))+0;	/*rnd_range algo: (rand()%(max-min+1))+min*/
	int_map[i].flags|=MAP_SNODE;
	int_map[i].flags&=~MAP_VOID;
	if(b) {
		r++;
		memset(&rtmp, '\0', sizeof(map_rnode));
		rtmp.r_node=(u_int *)&int_map[back_link];
		rtmp.rtt.tv_usec=back_link_rtt;
		//printf("Node %d -> Adding rnode %d (back link)\n", i, back_link);
		rnode_add(&int_map[i], &rtmp);
		b=0;
	}
	/*printf("Creating %d links for the node %d\n",  r, i);*/
	for(e=0; e<r; e++) { /*It's e<r and not e<=r because we've already added the back_link rnode at r position*/
		memset(&rtmp, '\0', sizeof(map_rnode));
random_node:
		//printf("rn\n");
		while((rnode_rnd=(random()%(MAXGROUPNODE-0))+0) == i);
		for(b=0; b<int_map[i].links; b++)
			if((map_node *)&int_map[rnode_rnd] == (map_node *)int_map[i].r_node[b].r_node) {
				//printf("goto random_node;\n");
				goto random_node;
			}

		rtmp.r_node=(u_int *)&int_map[rnode_rnd];
		ms_rnd=(random()%((MAXRTT*1000)-0))+0;
		rtmp.rtt.tv_usec=ms_rnd*1000;
		//printf("Node %d -> Adding rnode %d\n", i, rnode_rnd);
		rnode_add(&int_map[i], &rtmp);

		/*Does exist the node "rnode_rnd" added as rnode?*/
		if(int_map[rnode_rnd].flags & MAP_VOID)	
			/*No, let's create it*/
			gen_rnd_map(rnode_rnd, i, rtmp.rtt.tv_usec);
		else {
			/*It does, let's check if it has a link to me*/
			int c=0;
			for(b=0; b<int_map[rnode_rnd].links; b++)
				if((map_node *)int_map[rnode_rnd].r_node[b].r_node == &int_map[i]) {
					c=1;
					break;
				}
			if(!c) {
				/*We create the back link from rnode_rnd to me (i)*/
				memset(&rtmp, '\0', sizeof(map_rnode));
				rtmp.r_node=(u_int *)&int_map[i];
				rtmp.rtt.tv_usec=ms_rnd*1000;
				//printf("Node %d -> Adding rnode %d (front link)\n", rnode_rnd,i);
				rnode_add(&int_map[rnode_rnd], &rtmp);
			}
		}
	}
}

int store_tracer_pkt(struct q_opt *qopt)
{
	int x, pkt, to=qopt->q.to;

	pthread_mutex_lock(&mutex[to]);	
	pkt=pkt_dbc[to];
	pkt_dbc[to]++;
	pthread_mutex_unlock(&mutex[to]);	

	if(!pkt)
		pkt_db[to]=xmalloc(sizeof(struct q_opt *));
	else
		pkt_db[to]=xrealloc(pkt_db[to], sizeof(struct q_opt *)*pkt_dbc[to]);

	pkt_db[to][pkt]=xmalloc(sizeof(struct q_pkt));
	memset(pkt_db[to][pkt], 0, sizeof(struct q_pkt));
	pkt_db[to][pkt]->from=qopt->q.from;
	pkt_db[to][pkt]->routes=qopt->q.routes+1;
	if(pkt_db[to][pkt]->routes) {
		pkt_db[to][pkt]->tracer=xmalloc(sizeof(short)*pkt_db[to][pkt]->routes);
		for(x=0; x<qopt->q.routes; x++)
			pkt_db[to][pkt]->tracer[x]=qopt->q.tracer[x];
		/*Let's add our entry in the tracer pkt*/
		pkt_db[to][pkt]->tracer[pkt_db[to][pkt]->routes-1]=to;
	}
	pkt_db[to][pkt]->op=qopt->q.op;
	pkt_db[to][pkt]->broadcast=qopt->q.broadcast;

	return pkt;
}

void *send_qspn_backpro(void *argv)
{
	struct q_opt *qopt=(struct q_opt *)argv, *nopt;
	int x, dst, pkt, to=qopt->q.to;

	usleep(qopt->sleep);
	fprintf(stderr, "%u: qspn_backpro from %d to %d\n", pthread_self(), qopt->q.from, to);

	/*Now we store the received pkt in our pkt_db*/
	pkt=store_tracer_pkt(qopt);	

	/*We've arrived... finally*/
	if(int_map[to].flags & QSPN_STARTER) {
		fprintf(stderr, "%u: qspn_backpro: We've arrived... finally\n", pthread_self());
		return;
	}

	for(x=0; x<int_map[to].links; x++) {
		if((map_node *)int_map[to].r_node[x].r_node == &int_map[qopt->q.from]) 
			continue;

		if(int_map[to].r_node[x].flags & QSPN_CLOSED) {
			dst=((void *)int_map[to].r_node[x].r_node - (void *)int_map)/sizeof(map_node);

			gbl_stat.total_pkts++;
			node_stat[to].total_pkts++;

			nopt=xmalloc(sizeof(struct q_opt));
			memset(nopt, 0, sizeof(struct q_opt));
			nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
			nopt->q.to=dst;
			nopt->q.from=to;
			nopt->q.routes=pkt_db[to][pkt]->routes;
			nopt->q.tracer=pkt_db[to][pkt]->tracer;
			nopt->q.broadcast=pkt_db[to][pkt]->broadcast;
			nopt->join=qopt->join;

			/* gbl_stat.qspn_backpro++;
			 * node_stat[to].qspn_backpro++;
			 */
			nopt->q.op=OP_BACKPRO;
			thread_joint(qopt->join, send_qspn_backpro, (void *)nopt);
		}
	}
	xfree(qopt);
	total_threads--;
}

void *send_qspn_reply(void *argv)
{
	struct q_opt *qopt=(struct q_opt *)argv, *nopt;
	int x, dst, pkt, to=qopt->q.to;

	usleep(qopt->sleep);
	fprintf(stderr, "%u: qspn_reply from %d to %d\n", pthread_self(), qopt->q.from, to);

	/*Bad old broadcast pkt*/
	if(qopt->q.broadcast <= int_map[to].broadcast[qopt->q.from]) {
		fprintf(stderr, "%u: DROPPED old brdcast: q.broadcast: %d, qopt->q.from broadcast: %d\n", pthread_self(), qopt->q.broadcast, int_map[to].broadcast[qopt->q.from]);
		return;
	} else
		int_map[to].broadcast[qopt->q.from]=qopt->q.broadcast;

	/*Now we store the received pkt in our pkt_db*/
	pkt=store_tracer_pkt(qopt);	

	/*Let's keep broadcasting*/
	for(x=0; x<int_map[to].links; x++) {	
		if((map_node *)int_map[to].r_node[x].r_node == &int_map[qopt->q.from]) 
			continue;

		dst=((void *)int_map[to].r_node[x].r_node - (void *)int_map)/sizeof(map_node);

		gbl_stat.total_pkts++;
		node_stat[to].total_pkts++;

		nopt=xmalloc(sizeof(struct q_opt));
		memset(nopt, 0, sizeof(struct q_opt));
		nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
		nopt->q.to=dst;
		nopt->q.from=to;
		nopt->q.routes=pkt_db[to][pkt]->routes;
		nopt->q.tracer=pkt_db[to][pkt]->tracer;
		nopt->q.broadcast=pkt_db[to][pkt]->broadcast;
		nopt->join=qopt->join;

		/*
		gbl_stat.qspn_replies++;
		node_stat[to].qspn_replies++;
		*/
		nopt->q.op=OP_REPLY;
		thread_joint(qopt->join, send_qspn_reply, (void *)nopt);
	}
	xfree(qopt);
	total_threads--;
}

void *send_qspn_pkt(void *argv)
{
	struct q_opt *qopt=(struct q_opt *)argv, *nopt;
	int x, i=0, dst, pkt, to=qopt->q.to;

	usleep(qopt->sleep);
	fprintf(stderr, "%u: qspn_pkt from %d to %d\n", pthread_self(), qopt->q.from, to);
	
	pkt=store_tracer_pkt(qopt);	
	
	if(int_map[to].flags & QSPN_STARTER) {
		fprintf(stderr, "%u: qspn_pkt: We received a qspn_pkt, but we are the QSPN_STARTER!!\n", pthread_self());
		return;
	}
	
	for(x=0; x<int_map[to].links; x++) {
		if((map_node *)int_map[to].r_node[x].r_node == &int_map[qopt->q.from]) {
			int_map[to].r_node[x].flags|=QSPN_CLOSED;
			/*fprintf(stderr, "%u: node:%d->rnode %d closed\n", pthread_self(), to, x);*/
		}
		if(!(int_map[to].r_node[x].flags & QSPN_CLOSED))
			i++;
	}
	/*Shall we send a QSPN_REPLY?*/
	if(!i && !(int_map[to].flags & QSPN_REPLIED) && !(int_map[to].flags & QSPN_STARTER)) {
		/*W00t I'm an extreme node!*/
		fprintf(stderr, "%u: W00t I'm an extreme node!\n", pthread_self());
		
		int_map[to].flags|=QSPN_REPLIED;
		for(x=0; x<int_map[to].links; x++) {	
			if((map_node *)int_map[to].r_node[x].r_node == &int_map[qopt->q.from]) 
				continue;

			/*We've to clear the closed link*/
			int_map[to].r_node[x].flags&=~QSPN_CLOSED;

			dst=((void *)int_map[to].r_node[x].r_node - (void *)int_map)/sizeof(map_node);
			gbl_stat.total_pkts++;
			node_stat[to].total_pkts++;

			nopt=xmalloc(sizeof(struct q_opt));
			memset(nopt, 0, sizeof(struct q_opt));
			nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
			nopt->q.to=dst;
			nopt->q.from=to;
			nopt->q.routes=pkt_db[to][pkt]->routes;
			nopt->q.tracer=pkt_db[to][pkt]->tracer;
			nopt->q.op=OP_REPLY;
			int_map[to].broadcast[to]++;
			nopt->q.broadcast=int_map[to].broadcast[to];
			nopt->join=qopt->join;

			gbl_stat.qspn_replies++;
			node_stat[to].qspn_replies++;
			fprintf(stderr, "%u: Sending a qspn_reply to %d\n", pthread_self(), dst);
			thread_joint(qopt->join, send_qspn_reply, (void *)nopt);
			xfree(qopt);
			return;
		}
	}

	for(x=0; x<int_map[to].links; x++) {	
		if((map_node *)int_map[to].r_node[x].r_node == &int_map[qopt->q.from]) 
			continue;
#ifndef BACKPRO
		if(int_map[to].r_node[x].flags & QSPN_CLOSED)
			continue;
#endif

		dst=((void *)int_map[to].r_node[x].r_node - (void *)int_map)/sizeof(map_node);
		gbl_stat.total_pkts++;
		node_stat[to].total_pkts++;

		nopt=xmalloc(sizeof(struct q_opt));
		memset(nopt, 0, sizeof(struct q_opt));
		nopt->q.from=to;
		nopt->q.to=dst;
		nopt->q.routes=pkt_db[to][pkt]->routes;
		nopt->q.tracer=pkt_db[to][pkt]->tracer;
		nopt->sleep=int_map[to].r_node[x].rtt.tv_usec;
		nopt->q.broadcast=pkt_db[to][pkt]->broadcast;
		nopt->join=qopt->join;

		if(int_map[to].r_node[x].flags & QSPN_CLOSED && !(int_map[to].r_node[x].flags & QSPN_BACKPRO)) {
#ifdef BACKPRO
			gbl_stat.qspn_backpro++;
			node_stat[to].qspn_backpro++;
			nopt->q.op=OP_BACKPRO;
			int_map[to].r_node[x].flags&=QSPN_BACKPRO;
			thread_joint(qopt->join, send_qspn_backpro, (void *)nopt);
#else
			0;
#endif
		} else {
			gbl_stat.qspn_requests++;
			node_stat[to].qspn_requests++;
			nopt->q.op=OP_REQUEST;
			thread_joint(qopt->join, send_qspn_pkt, (void *)nopt);
		}
	}
	xfree(qopt);
	total_threads--;
}

void collect_data(void)
{
	int i, x, e;

	fprintf(stderr, "Collecting the data!\n");
	for(i=0; i<MAXGROUPNODE; i++)
		for(e=0; e<pkt_dbc[i]; e++)
			for(x=0; x<pkt_db[i][e]->routes; x++)
				if((rt_stat[i][pkt_db[i][e]->tracer[x]]++)==1)
					rt_total[i]++;
}

void *show_temp_stat(void *null)
{
	FILE *fd=stdout;
	while(1) {
		sleep(5);
		fprintf(fd, "Total_threads: %d\n", total_threads);		
		fprintf(fd, "Gbl_stat{\n\ttotal_pkts: %d\n\tqspn_requests: %d"
				"\n\tqspn_replies: %d\n\tqspn_backpro: %d }\n\n",
				gbl_stat.total_pkts, gbl_stat.qspn_requests,
				gbl_stat.qspn_replies, gbl_stat.qspn_backpro);
	}
}

int print_map(map_node *map, char *map_file)
{
	int x,e, node;
	FILE *fd;

	fd=fopen(map_file, "w");
	fprintf(fd,"--- map ---\n");
	for(x=0; x<MAXGROUPNODE; x++) {
		fprintf(fd, "Node %d\n",x);
		for(e=0; e<map[x].links; e++) {
			node=((void *)map[x].r_node[e].r_node - (void *)map)/sizeof(map_node);
			fprintf(fd, "        -> %d\n", node);
		}

		fprintf(fd, "--\n");
	}
	fclose(fd);
	return 0;
}

int lgl_print_map(map_node *map, char *lgl_mapfile)
{
	int x,e,i, c=0, d, node;
	FILE *lgl;
	
	lgl=fopen(lgl_mapfile, "w");

	for(x=0; x<MAXGROUPNODE; x++) {
		fprintf(lgl, "# %d\n", x);
		for(e=0; e<map[x].links; e++) {
			c=0;
			for(i=0; i<x; i++)
				if(&map[i] == (map_node *)map[x].r_node[e].r_node) {
					for(d=0; d<map[i].links; d++)
						if((map_node *)map[i].r_node[d].r_node == &map[x]) {
							c=1;
							break;
						}
					if(c)
						break;
				}
			if(!c) {
				node=((void *)map[x].r_node[e].r_node - (void *)map)/sizeof(map_node);
				fprintf(lgl, "%d %d\n",node, map[x].r_node[e].rtt.tv_usec);
			}
		}
	}
	fclose(lgl);
	return 0;
}

void print_data(char *file)
{
	int i, x, e;
	FILE *fd;

	fprintf(stderr, "Saving the d4ta\n");
	fd=fopen((file), "w");

	fprintf(fd, "---- Test dump n. 6 ----\n");

	for(i=0; i<MAXGROUPNODE; i++)
		if(rt_total[i]<MAXGROUPNODE && int_map[i].links) 
			fprintf(fd,"*WARNING* The node %d has only %d/%d routes *WARNING*\n", i, rt_total[i], MAXGROUPNODE);

	fprintf(fd, "- Gbl_stat{\n\ttotal_pkts: %d\n\tqspn_requests: %d"
			"\n\tqspn_replies: %d\n\tqspn_backpro: %d }, QSPN finished in :%d seconds\n",
			gbl_stat.total_pkts, gbl_stat.qspn_requests,
			gbl_stat.qspn_replies, gbl_stat.qspn_backpro, time_stat);

	fprintf(fd, "- Total routes: \n");
	for(i=0; i<MAXGROUPNODE; i++) {	
		fprintf(fd, "Node: %d { ", i);
		for(x=0; x<MAXGROUPNODE; x++) {
			if(!int_map[x].links)
				fprintf(fd, "(%d)NULL ", x);
			else
				fprintf(fd, "(%d)%d ", x,rt_stat[i][x]);

			if(!x%20 && x)
				fprintf(fd, "\n           ");
		}
		fprintf(fd, "}\n");
	}

	fprintf(fd, "\n--\n\n");
	fprintf(fd, "- Node single stats: \n");

	for(i=0; i<MAXGROUPNODE; i++)
		fprintf(fd, "%d_stat{\n\ttotal_pkts: %d\n\tqspn_requests: %d\n\t"
				"qspn_replies: %d\n\tqspn_backpro: %d }\n", i,
				node_stat[i].total_pkts, node_stat[i].qspn_requests,
				node_stat[i].qspn_replies, node_stat[i].qspn_backpro);

	fprintf(fd, "- Pkts dump: \n");
	for(i=0; i<MAXGROUPNODE; i++) {
		for(x=0; x<pkt_dbc[i]; x++) {
			fprintf(fd, "(%d) { op: %d, from: %d, broadcast: %d, ",
					i, pkt_db[i][x]->op, pkt_db[i][x]->from,
					pkt_db[i][x]->broadcast);
			fprintf(fd, "tracer: ");
			for(e=0; e<pkt_db[i][x]->routes; e++) {
				fprintf(fd, "%d -> ",pkt_db[i][x]->tracer[e]);
				if(!x%16 && x)
					fprintf(fd, "\n");
			}
			fprintf(fd, "}\n");
		}
	}
	fclose(fd);
}

void clear_all(void)
{
	fprintf(stderr, "Clearing all the dirty\n");
	memset(&gbl_stat, 0, sizeof(struct stat));
	memset(&node_stat, 0, sizeof(struct stat)*MAXGROUPNODE);
	memset(&pkt_db, 0, sizeof(struct q_pkt)*MAXGROUPNODE);
	memset(&pkt_dbc, 0, sizeof(int)*MAXGROUPNODE);
	memset(&rt_stat, 0, sizeof(short)*MAXGROUPNODE*MAXGROUPNODE);
	memset(&rt_total, 0, sizeof(short)*MAXGROUPNODE);
}

int main(int argc, char **argv)
{
	struct q_opt *nopt;
	int i, r, e, x;
	time_t start, end;

	log_init(argv[0], 1, 1);
	clear_all();

	for(i=0; i<MAXGROUPNODE; i++) 
		pthread_mutex_init(&mutex[i], NULL);

	if(argc>1) {
		if(!(int_map=load_map(argv[1]))) {
			printf("Error! Cannot load the map\n");
			exit(1);
		}
		printf("Map loaded. Printing it... \n");
		print_map(int_map, "QSPN-map.load");
		lgl_print_map(int_map, "QSPN-map.lgl.load");
	} else {
		int_map=init_map(sizeof(map_node)*MAXGROUPNODE);
		printf("Generating a random map...\n");
		srandom(time(0));
		i=(random()%(MAXGROUPNODE-0))+0;
		gen_rnd_map(i, -1, 0);
		for(x=0; x<MAXGROUPNODE; x++)
			rnode_rtt_order(&int_map[x]);
		printf("Map generated. Printing it... \n");
		print_map(int_map, "QSPN-map");
		lgl_print_map(int_map, "QSPN-map.lgl");
		int_map[i].flags|=MAP_ME;
		printf("Saving the map to QSPN-map.raw\n");
		save_map(int_map, &int_map[i], "QSPN-map.raw");
	}
	
	printf("Running the first test...\n");
	thread_joint(0, show_temp_stat, NULL);
	if(argc > 2)
		r=atoi(argv[2]);
	else
		r=(random()%(MAXGROUPNODE-0))+0;
	printf("Starting the QSPN spreading from node %d\n", r);
	int_map[r].flags|=QSPN_STARTER;
	start=time(0);
	for(x=0; x<int_map[r].links; x++) {
		gbl_stat.total_pkts++;
		node_stat[r].total_pkts++;

		nopt=xmalloc(sizeof(struct q_opt));
		memset(nopt, 0, sizeof(struct q_opt));
		nopt->q.from=r;
		nopt->q.to=((void *)int_map[r].r_node[x].r_node - (void *)int_map)/sizeof(map_node);
		nopt->q.tracer=xmalloc(sizeof(short));
		nopt->q.tracer[0]=r;
		nopt->q.routes=1;
		nopt->sleep=int_map[r].r_node[x].rtt.tv_usec;
		nopt->q.broadcast=0;
		nopt->join=0;

		gbl_stat.qspn_requests++;
		node_stat[r].qspn_requests++;
		nopt->q.op=OP_REQUEST;
		if(x == int_map[r].links-1)
			nopt->join=1;
		
		thread_joint(nopt->join, send_qspn_pkt, (void *)nopt);
	}
	end=time(0);
	int_map[r].flags&=~QSPN_STARTER;
		
	printf("Saving the data to QSPN1...\n");
	collect_data();
	print_data("QSPN1");
	for(x=0; x<MAXGROUPNODE; x++) {
		for(e=0; e<pkt_dbc[x]; e++) {
			xfree(pkt_db[x][e]->tracer);
			xfree(pkt_db[x][e]);
		}
		xfree(pkt_db[x]);
	}
	clear_all();
	printf("All done yeah\n");
	exit(0);
}
