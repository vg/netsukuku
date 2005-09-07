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

/* from linux/ipv6.h */
struct in6_ifreq {
	struct in6_addr ifr6_addr;
	uint32_t        ifr6_prefixlen;
	int             ifr6_ifindex;
};


const char *get_dev(int *dev_idx);
int set_dev_up(char *dev);
int set_dev_down(char *dev);
int set_flags(char *dev, u_int flags, u_int mask);
const char *if_init(char *dev, int *dev_idx);
int set_dev_ip(inet_prefix ip, char *dev);
int ip_addr_flush(int family, char *dev, int scope);
