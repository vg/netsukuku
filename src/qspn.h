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

#define MAX_CONC_QSPN	5	/*MAX CONCURRENT QSPN*/

/* This struct keeps tracks of the qspn_pkts sent or
 * received by our rnodes*/
struct qspn_buffer
{
	u_int	replies;	/*How many replies we forwarded*/
	u_short *replier;	/*Who has sent these replies (qspn_sub_id)*/
	u_short	*flags;
};
struct qspn_queue *qspn_b;


int 	q_id;		/*The qspn_id we are processing*/
int	q_time;
