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

pthread_mutex_t mutex[MAXGROUPNODE];
pthread_attr_t t_attr;

map_node *int_map;

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
#define OP_REPLY	69
#define OP_BACKPRO	66

struct q_pkt
{
	short from;
	short to;
	int   broadcast;
	char  op;
	short *tracer;
	short routes;
};

struct q_pkt **pkt_db[MAXGROUPNODE];
int pkt_dbc[MAXGROUPNODE];

struct q_opt
{
	struct q_pkt q;
	int sleep;
};

void gen_rnd_map(int start_node);
int store_tracer_pkt(struct q_opt *qopt);
void *send_qspn_backpro(void *argv);
void *send_qspn_reply(void *argv);
void *send_qspn_pkt(void *argv);
