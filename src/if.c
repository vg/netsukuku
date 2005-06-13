/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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


#include "includes.h"

#include "inet.h"
#include "if.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "xmalloc.h"
#include "log.h"

extern int errno;

/* 
 * get_dev: It returs the first dev it finds up and sets `*dev_ids' to the
 * device's index. On error NULL is returned.
 */
const char *get_dev(int *dev_idx) 
{
	int idx;

	if((idx=ll_first_up_if()) == -1) {
		error("Couldn't find \"up\" devices. Set one dev \"up\", or "
				"specify the device name in the options.");
		return 0;
	}
	if(dev_idx)
		*dev_idx=idx;
	return ll_index_to_name(idx);
}

int set_dev_up(char *dev)
{
	u_int mask=0, flags=0;
	
	mask |= IFF_UP;
	flags |= IFF_UP;
	return set_flags(dev, flags, mask);
}

int set_dev_down(char *dev)
{
	u_int mask=0, flags=0;
	
	mask |= IFF_UP;
	flags &= ~IFF_UP;
	return set_flags(dev, flags, mask);
}

int set_flags(char *dev, u_int flags, u_int mask)
{
	struct ifreq ifr;
	int s;

	strcpy(ifr.ifr_name, dev);
	if((s=new_socket(AF_INET)) < 0) {
		error("Error while setting \"%s\" flags: Cannot open socket", dev);
		return -1;
	}

	if(ioctl(s, SIOCGIFFLAGS, &ifr)) {
		error("Error while setting \"%s\" flags: %s", dev, strerror(errno));
		close(s);
		return -1;
	}

	ifr.ifr_flags &= ~mask;
	ifr.ifr_flags |= mask&flags;
	if(ioctl(s, SIOCSIFFLAGS, &ifr)) {
		error("Error while setting \"%s\" flags: %s", dev, strerror(errno));
		close(s);
		return -1;
	}
	close(s);
	return 0;
}

/*
 * if_init: It initializes the if to be used by Netsukuku.
 * It sets `*dev_idx' to the relative idx of `dev' and returns the device name.
 * If an error occured it returns NULL.*/
const char *if_init(char *dev, int *dev_idx)
{
	struct rtnl_handle rth;
	int idx;

	if (rtnl_open(&rth, 0) < 0) {
		error("Cannot open the rtnetlink socket to talk to the kernel's "
				"soul");
		return NULL;
	}
	ll_init_map(&rth);

	if(dev[0] != 0) {
		if ((*dev_idx = idx = ll_name_to_index(dev)) == 0) {
			error("if_init: Cannot find device \"%s\"\n", dev);
			return NULL;
		}
	} else 
		if(!(dev=(char *)get_dev(dev_idx)))
				return NULL;
	
	if(set_dev_up(dev))
		return NULL;	
	
	return (const char *)dev;
}	

int set_dev_ip(inet_prefix ip, char *dev)
{
	int s;
	char *p;

	if(ip.family == AF_INET) {
		struct ifreq req;

		if((s=new_socket(AF_INET)) < 0) {
			error("Error while setting \"%s\" ip: Cannot open socket", dev);
			return -1;
		}

		strncpy(req.ifr_name, dev, IFNAMSIZ);
		inet_to_sockaddr(&ip, 0, &req.ifr_addr, 0);

		if(ioctl(s, SIOCSIFADDR, &req)) {
			error("Error while setting \"%s\" ip: %s", dev, strerror(errno));
			close(s);
			return -1;
		}
	} else if(ip.family == AF_INET6) {
		struct in6_ifreq req6;
		struct sockaddr sa;

		req6.ifr6_ifindex=ll_name_to_index(dev);
		req6.ifr6_prefixlen=0;
		inet_to_sockaddr(&ip, 0, &sa, 0);
		p=(char *)sa.sa_data+sizeof(u_short)+sizeof(u_int);
		memcpy(&req6.ifr6_addr, p, ip.len);

		if(ioctl(s, SIOCSIFADDR, &req6)) {
			error("Error while setting \"%s\" ip: %s", dev, strerror(errno));
			close(s);
			return -1;
		}

	}

	close(s);
	return 0;
}
