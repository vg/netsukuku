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

#include <sys/types.h>

/*Used for: ip.data[0]=HOOKING_IP;*/
#define HOOKING_IP  0x8f000001      /* 128.0.0.1   */
#define HOOKING_IP6 0x8f000001

struct free_ips
{
	u_short  	gid;
	u_short		ips;
	inet_prefix 	ipstart;
	struct timeval  qtime;	/*qspn round time: how many seconds passed away
					  since the previous qspn round*/
};

int get_free_ips(inet_prefix to, struct free_ips *fi_hdr, int *ips):
int put_free_ips(PACKET rq_pkt);
map_node *get_int_map(inet_prefix to, map_node *new_root);
int netsukuku_hook(char *dev);
int snode_transfrom();
