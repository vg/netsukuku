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

#include "includes.h"

#include "libnetlink.h"
#include "inet.h"
#include "krnl_route.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "xmalloc.h"
#include "log.h"

int route_exec(int route_cmd, int route_type, unsigned flags, inet_prefix to, 
		struct nexthop *nhops, char *dev, u_char table);

int route_add(int type, inet_prefix to, struct nexthop *nhops, char *dev, u_char table)
{
	return route_exec(RTM_NEWROUTE, type, NLM_F_CREATE | NLM_F_EXCL, to, 
			nhops, dev, table);
}

int route_del(int type, inet_prefix to, struct nexthop *nhops, char *dev, u_char table)
{
	return route_exec(RTM_DELROUTE, type, 0, to, nhops, dev, table);
}

/*If it doesn't exist, CREATE IT! de ih oh oh*/
int route_replace(int type, inet_prefix to, struct nexthop *nhops, char *dev, u_char table)
{
	return route_exec(RTM_NEWROUTE, type, NLM_F_REPLACE | NLM_F_CREATE, to,
			nhops, dev, table);
}

int route_change(int type, inet_prefix to, struct nexthop *nhops, char *dev, u_char table)
{
	return route_exec(RTM_NEWROUTE, type, NLM_F_REPLACE, to, nhops, dev, 
			table);
}

int add_nexthops(struct nlmsghdr *n, struct rtmsg *r, struct nexthop *nhop)
{
	char buf[1024];
	struct rtattr *rta = (void*)buf;
	struct rtnexthop *rtnh;
	int i=0;

	rta->rta_type = RTA_MULTIPATH;
	rta->rta_len = RTA_LENGTH(0);
	rtnh = RTA_DATA(rta);

	if(!nhop[i+1].dev && !i) {
		/* Just one gateway */
		r->rtm_family = nhop[i].gw.family;
		addattr_l(n, sizeof(struct rt_request), RTA_GATEWAY, &nhop[i].gw.data, nhop[i].gw.len);
		return 0;
	}
	
	while (nhop[i].dev!=0) {
		memset(rtnh, 0, sizeof(*rtnh));
		rtnh->rtnh_len = sizeof(*rtnh);
		rta->rta_len += rtnh->rtnh_len;

		if (nhop[i].gw.len) {
			if(nhop[i].gw.family==AF_INET)
				rta_addattr32(rta, 4096, RTA_GATEWAY, nhop[i].gw.data[0]);
			else if(nhop[i].gw.family==AF_INET6)
				rta_addattr_l(rta, 4096, RTA_GATEWAY, nhop[i].gw.data, nhop[i].gw.len);
			rtnh->rtnh_len += sizeof(struct rtattr) + nhop[i].gw.len;
		}

		if (nhop[i].dev) 
			if ((rtnh->rtnh_ifindex = ll_name_to_index(nhop[i].dev)) == 0)
				fatal("%s:%d, Cannot find device \"%s\"\n", ERROR_POS, nhop[i].dev);

		if (nhop[i].hops == 0) {
			debug(DBG_NORMAL, "hops=%d is invalid. Using hops=255\n", nhop[i].hops);
			rtnh->rtnh_hops=255;
		} else
			rtnh->rtnh_hops = nhop[i].hops - 1;

		rtnh = RTNH_NEXT(rtnh);
		i++;
	}

	if (rta->rta_len > RTA_LENGTH(0))
		addattr_l(n, 1024, RTA_MULTIPATH, RTA_DATA(rta), RTA_PAYLOAD(rta));
	return 0;
}

int route_exec(int route_cmd, int route_type, unsigned flags, inet_prefix to, 
		struct nexthop *nhops, char *dev, u_char table)
{
	struct rt_request req;
	struct rtnl_handle rth;

	memset(&req, 0, sizeof(req));

	if(!table)
		table=RT_TABLE_MAIN;

	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.nh.nlmsg_flags = NLM_F_REQUEST|flags;
	req.nh.nlmsg_type = route_cmd;
	req.rt.rtm_family = AF_UNSPEC;
	req.rt.rtm_table = table;
	req.rt.rtm_protocol=RTPROT_KERNEL;
	req.rt.rtm_scope = RT_SCOPE_NOWHERE;

	if (route_cmd != RTM_DELROUTE) {
		req.rt.rtm_protocol = RTPROT_NETSUKUKU;
		req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
		req.rt.rtm_type = RTN_UNICAST;
	}
	
	if(route_type)
		req.rt.rtm_type = route_type;
	
	if (rtnl_open(&rth, 0) < 0)
		return -1;

	if (dev || nhops) 
		ll_init_map(&rth);

	if (dev) {
		int idx;

		if ((idx = ll_name_to_index(dev)) == 0)
			fatal("%s:%d, Device \"%s\" doesn't really exist\n", ERROR_POS, dev);
		addattr32(&req.nh, sizeof(req), RTA_OIF, idx);
	}

	if (to.len) {
		req.rt.rtm_dst_len = to.bits;
		req.rt.rtm_family  = to.family;
		if(!to.data[0] && !to.data[1] && !to.data[2] && !to.data[3]) {
			/*Add the default gw*/
			req.rt.rtm_protocol=RTPROT_KERNEL;
		}

		addattr_l(&req.nh, sizeof(req), RTA_DST, &to.data, to.len);
	} 

	if(req.rt.rtm_type==RTN_LOCAL)
		req.rt.rtm_scope=RT_SCOPE_HOST;

	if(nhops) 
		add_nexthops(&req.nh, &req.rt, nhops);
	
	/*Finaly stage: <<Hey krnl, r u there?>>*/
	if (rtnl_talk(&rth, &req.nh, 0, 0, NULL, NULL, NULL) < 0)
		return -1;

	return 0;
}

int route_flush_cache(int family)
{
	int len, err;
	int flush_fd;
	char ROUTE_FLUSH_SYSCTL[]="/proc/sys/net/ipvX/route/flush";
	char *buf = "-1";

	len = strlen(buf);
	if(family==AF_INET)
		ROUTE_FLUSH_SYSCTL[17]='4';
	else if(family==AF_INET6)
		ROUTE_FLUSH_SYSCTL[17]='6';
	else
		return -1;

	flush_fd=open(ROUTE_FLUSH_SYSCTL, O_WRONLY);
	if (flush_fd < 0) {
		debug(DBG_NORMAL, "Cannot open \"%s\"\n", ROUTE_FLUSH_SYSCTL);
		return -1;
	}
		
	if ((err=write (flush_fd, (void *)buf, len)) == 0) {
		debug(DBG_NORMAL, "Warning: Route Cache not flushed\n");
		return -1;
	} else if(err==-1) {
		debug(DBG_NORMAL, "Cannot flush routing cache: %s\n", strerror(errno));
		return -1;
	}
	close(flush_fd);

	return 0;
}

/*Life is strange*/
