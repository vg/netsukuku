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

#include <sys/time.h>
#include "request.h"
#include "map.h"
#include "pkts.h"

#define MAX_RADAR_SCANS		10
#define MAX_RADAR_WAIT		10

#define RTT_DELTA		1	/*If the change delta of the new rtt is >= RTT_DELTA, 
					  the qspn_q.send_qspn will be set*/

int max_radar_wait=MAX_RADAR_WAIT;	/*How much we wait to store the received ECHO_REPLY pkts
					  and then to close the current radar session*/
int radar_scans=0;			/*How many ECHO_ME pkts we sent*/
int radar_scan_mutex;			/*A flag to see if we are already doing a scan*/
int my_echo_id=0;			
int send_qspn_now[MAX_LEVELS];		/*Shall we send the qspn in level? If yes send_qspn_now[level] is != 0*/

#define RADQ_VOID_RNODE		0
#define RADQ_EXT_RNODE		1

struct radar_queue
{
	struct radar_queue *next;
	struct radar_queue *prev;

	inet_prefix	ip;			/*Node's ip*/
	map_node       *node;			/*The node we are pinging*/
	quadro_group	quadg;			/*Node's data for the ext_map*/
						   
	char pongs;				/*The total pongs received*/
	struct timeval rtt[MAX_RADAR_SCANS];	/*The round rtt of each pong*/
	struct timeval final_rtt;		/*When all the rtt is filled, or when MAX_RADAR_WAIT is expired,
						  final_rtt will keep the average of all the rtts
						 */
};
struct radar_queue *radar_q;	/*the start of the linked list of radar_queue*/
struct timeval scan_start;	/*the start of the scan*/

void init_radar(void);
void close_radar(void);
void reset_radar(void);
void free_new_node(void);
struct radar_queue *find_ip_radar_q(map_node *node);
u_int *find_nnode_radar_q(inet_prefix *node);
void final_radar_queue(void);
void radar_update_map(void);
int add_radar_q(PACKET pkt);
int radar_scan(void);
int radar_recv_reply(PACKET pkt);
