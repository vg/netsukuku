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
int max_radar_wait=MAX_RADAR_WAIT;
int radar_scan_mutex;
int my_echo_id=0;

struct radar_queue
{
	map_node       *node;			/*The node we are pinging*/
	inet_prefix	ip;			/*Node's ip*/
	char pongs;				/*The total pongs received*/
	struct timeval rtt[MAX_RADAR_SCANS];	/*The round rtt of each pong*/
	struct timeval final_rtt;		/*When all the rtt is filled, or when MAX_RADAR_WAIT is passed, final_rtt
						  will keep the average of all the rtts
						 */
};

struct timeval scan_start;			/*the start of the scan*/
struct radar_queue *radar_q;
/*How many radar_queue are allocated in radar_q?*/
int radar_q_alloc;



void init_radar(void);
int find_free_radar_q(void);
int find_ip_radar_q(map_node node);
void final_radar_queue(void);
void radar_update_map(void);
int radar_scan(void);
int add_radar_q(PACKET pkt);
int radar_recv_reply(PACKET pkt);
