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
#include <fnmatch.h>

#include "inet.h"
#include "if.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "xmalloc.h"
#include "log.h"

extern int errno;

static struct
{       
        int ifindex;
        int family;
        int oneline;
        int showqueue;
        inet_prefix pfx;
        int scope, scopemask;
        int flags, flagmask;
        int up;
        char *label;
        int flushed;
        char *flushb;
        int flushp;
        int flushe; 
        struct rtnl_handle *rth;
} filter;


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

/*
 * set_dev_ip: Assign the given `ip' to the interface named `dev'
 * On success 0 is returned, -1 otherwise.
 */
int set_dev_ip(inet_prefix ip, char *dev)
{
	int s;

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
		struct sockaddr_in6 sin6;
		struct sockaddr *sa=(struct sockaddr *)&sin6;

		if((s=new_socket(AF_INET6)) < 0) {
			error("Error while setting \"%s\" ip: Cannot open socket", dev);
			return -1;
		}
		
		req6.ifr6_ifindex=ll_name_to_index(dev);
		req6.ifr6_prefixlen=0;
		inet_to_sockaddr(&ip, 0, sa, 0);
		memcpy(&req6.ifr6_addr, sin6.sin6_addr.s6_addr32, ip.len);

		if(ioctl(s, SIOCSIFADDR, &req6)) {
			error("Error while setting \"%s\" ip: %s", dev, strerror(errno));
			close(s);
			return -1;
		}

	}

	close(s);
	return 0;
}

/*
 * All the code below this point is ripped from iproute2/iproute.c
 * written by Alexey Kuznetsov, <kuznet@ms2.inr.ac.ru>.
 *
 * Modified lightly
 */
static int flush_update(void)
{                       
        if (rtnl_send(filter.rth, filter.flushb, filter.flushp) < 0) {
                error("Failed to send flush request: %s", strerror(errno));
                return -1;
        }               
        filter.flushp = 0;
        return 0;
}

int print_addrinfo(const struct sockaddr_nl *who, struct nlmsghdr *n, 
		   void *arg)
{
	struct ifaddrmsg *ifa = NLMSG_DATA(n);
	int len = n->nlmsg_len;
	struct rtattr * rta_tb[IFA_MAX+1];
	char b1[64];

	if (n->nlmsg_type != RTM_NEWADDR && n->nlmsg_type != RTM_DELADDR)
		return 0;
	len -= NLMSG_LENGTH(sizeof(*ifa));
	if (len < 0) {
		error("BUG: wrong nlmsg len %d\n", len);
		return -1;
	}

	if (filter.flushb && n->nlmsg_type != RTM_NEWADDR)
		return 0;

	parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa)));

	if (!rta_tb[IFA_LOCAL])
		rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
	if (!rta_tb[IFA_ADDRESS])
		rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];

	if (filter.ifindex && filter.ifindex != ifa->ifa_index)
		return 0;
	if ((filter.scope^ifa->ifa_scope)&filter.scopemask)
		return 0;
	if ((filter.flags^ifa->ifa_flags)&filter.flagmask)
		return 0;
	if (filter.label) {
		const char *label;
		if (rta_tb[IFA_LABEL])
			label = RTA_DATA(rta_tb[IFA_LABEL]);
		else
			label = ll_idx_n2a(ifa->ifa_index, b1);
		if (fnmatch(filter.label, label, 0) != 0)
			return 0;
	}
	if (filter.pfx.family) {
		if (rta_tb[IFA_LOCAL]) {
			inet_prefix dst;
			memset(&dst, 0, sizeof(dst));
			dst.family = ifa->ifa_family;
			memcpy(&dst.data, RTA_DATA(rta_tb[IFA_LOCAL]), 
					RTA_PAYLOAD(rta_tb[IFA_LOCAL]));
			if (inet_addr_match(&dst, &filter.pfx, filter.pfx.bits))
				return 0;
		}
	}

	if (filter.flushb) {
		struct nlmsghdr *fn;
		if (NLMSG_ALIGN(filter.flushp) + n->nlmsg_len > filter.flushe) {
			if (flush_update())
				return -1;
		}
		fn = (struct nlmsghdr*)(filter.flushb + NLMSG_ALIGN(filter.flushp));
		memcpy(fn, n, n->nlmsg_len);
		fn->nlmsg_type = RTM_DELADDR;
		fn->nlmsg_flags = NLM_F_REQUEST;
		fn->nlmsg_seq = ++filter.rth->seq;
		filter.flushp = (((char*)fn) + n->nlmsg_len) - filter.flushb;
		filter.flushed++;
	}

	return 0;
}

struct nlmsg_list
{
        struct nlmsg_list *next;
        struct nlmsghdr   h;
};

static int store_nlmsg(const struct sockaddr_nl *who, struct nlmsghdr *n,
                       void *arg)
{
        struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
        struct nlmsg_list *h;
        struct nlmsg_list **lp;

        h = malloc(n->nlmsg_len+sizeof(void*));
        if (h == NULL)
                return -1;

        memcpy(&h->h, n, n->nlmsg_len);
        h->next = NULL;

        for (lp = linfo; *lp; lp = &(*lp)->next) /* NOTHING */;
        *lp = h;

        ll_remember_index((struct sockaddr_nl *)who, n, NULL);
        return 0;
}

int ip_addr_flush(int family, char *dev, int scope)
{
	struct nlmsg_list *linfo = NULL;
	struct rtnl_handle rth;
	char *filter_dev = NULL;

	memset(&filter, 0, sizeof(filter));
	filter.showqueue = 1;

	filter.family = family;
	filter_dev = dev;

	if (rtnl_open(&rth, 0) < 0)
		return -1;

	if (rtnl_wilddump_request(&rth, family, RTM_GETLINK) < 0) {
		error("Cannot send dump request: %s", strerror(errno));
		return -1;
	}

	if (rtnl_dump_filter(&rth, store_nlmsg, &linfo, NULL, NULL) < 0) {
		error("Dump terminated");
		return -1;
	}

	filter.ifindex = ll_name_to_index(filter_dev);
	if (filter.ifindex <= 0) {
		error("Device \"%s\" does not exist.", filter_dev);
		return -1;
	}

	int round = 0;
	char flushb[4096-512];

	filter.flushb = flushb;
	filter.flushp = 0;
	filter.flushe = sizeof(flushb);
	filter.rth = &rth;
        filter.scopemask = -1;
	filter.scope = scope;
	
	for (;;) {
		if (rtnl_wilddump_request(&rth, filter.family, RTM_GETADDR) < 0) {
			error("Cannot send dump request: %s", strerror(errno));
			return -1;
		}
		filter.flushed = 0;
		if (rtnl_dump_filter(&rth, print_addrinfo, stdout, NULL, NULL) < 0) {
			error("Flush terminated: %s", errno);
			return -1;
		}
		if (filter.flushed == 0)
			return 0;

		round++;
		if (flush_update() < 0)
			return -1;
	}
}
