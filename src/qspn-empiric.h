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


#include <pthread.h>
#include "map.h"

/*I use these define to activate/deactivate the different parts of QSPN*/
#undef Q_BACKPRO
#define Q_OPEN
#undef NO_JOINT

pthread_mutex_t mutex[MAXGROUPNODE];
int total_threads=0, disable_joint=0;

map_node *int_map;

/*This struct keeps tracks of the qspn_pkts sent or received by our rnodes*/
struct qspn_queue
{
	int 	q_id;			/*qspn_id*/
	u_short replier[MAXGROUPNODE];	/*Who has sent these repliesi (qspn_sub_id)*/
	u_short	flags[MAXGROUPNODE];
}*qspn_q[MAXGROUPNODE];

struct stat
{
	int total_pkts;
	int qspn_requests;
	int qspn_replies;
	int qspn_backpro;
};

int time_stat;
struct stat gbl_stat;
struct stat node_stat[MAXGROUPNODE];
short rt_stat[MAXGROUPNODE][MAXGROUPNODE];
short rt_total[MAXGROUPNODE];


#define OP_REQUEST 	82
#define OP_CLOSE 	OP_REQUEST
#define OP_OPEN 	28
#define OP_REPLY	69
#define OP_BACKPRO	66

#define QPKT_REPLY	1

struct q_pkt
{
	int q_id;
	int q_sub_id;
	short from;
	short to;
	int   broadcast;
	char  op;
	char  flags;
	short *tracer;
	short routes;
};

struct q_pkt **pkt_db[MAXGROUPNODE];
int pkt_dbc[MAXGROUPNODE];

struct q_opt
{
	struct q_pkt q;
	int sleep;
	int join;
};

void thread_joint(int joint, void * (*start_routine)(void *), void *nopt);
void gen_rnd_map(int start_node, int back_link, int back_link_rtt);
int print_map(map_node *map, char *map_file);
void *show_temp_stat(void *);
void print_data(char *file);
int store_tracer_pkt(struct q_opt *qopt);
void *send_qspn_backpro(void *argv);
void *send_qspn_reply(void *argv);
void *send_qspn_pkt(void *argv);
