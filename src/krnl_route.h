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

#define RTPROT_NETSUKUKU     15

struct nexthop 
{
	inet_prefix gw;
	u_char *dev;
	u_char hops;
};


int route_add(inet_prefix to, struct nexthop *nhops, char *dev, u_char table);
int route_del(inet_prefix to, struct nexthop *nhops, char *dev, u_char table);
int route_replace(inet_prefix to, struct nexthop *nhops, char *dev, u_char table);
int route_change(inet_prefix to, struct nexthop *nhops, char *dev, u_char table);
int add_nexthops(struct nlmsghdr *n, struct rtmsg *r, struct nexthop *nhop);
int route_exec(int route_cmd, unsigned flags, inet_prefix to, 
		struct nexthop *nhops, char *dev, u_char table);
int route_flush_cache(int family);
