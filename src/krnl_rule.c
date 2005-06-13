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

#include "libnetlink.h"
#include "inet.h"
#include "krnl_route.h"
#include "krnl_rule.h"
#include "xmalloc.h"
#include "log.h"

int rule_add(inet_prefix from, inet_prefix to, char *dev, int prio, u_char table)
{
	return rule_exec(RTM_NEWRULE, from, to, dev, prio, table);
}

int rule_del(inet_prefix from, inet_prefix to, char *dev, int prio, u_char table)
{
	return rule_exec(RTM_DELRULE, from, to, dev, prio, table);
}

int rule_exec(int rtm_cmd, inet_prefix from, inet_prefix to, char *dev, int prio, u_char table)
{
	struct {
		struct nlmsghdr 	nh;
		struct rtmsg 		rt;
		char   			buf[1024];
	} req;
	struct rtnl_handle rth;

	memset(&req, 0, sizeof(req));

	if(!table)
		table=RT_TABLE_MAIN;
	
	req.nh.nlmsg_type = rtm_cmd;
	req.nh.nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));
	req.nh.nlmsg_flags = NLM_F_REQUEST;
	req.rt.rtm_scope = RT_SCOPE_UNIVERSE;
	req.rt.rtm_type = RTN_UNSPEC;
	req.rt.rtm_family = AF_UNSPEC;
	req.rt.rtm_protocol = RTPROT_NETSUKUKU;
	req.rt.rtm_table = table;

	if (rtm_cmd == RTM_NEWRULE) {
		req.nh.nlmsg_flags |= NLM_F_CREATE|NLM_F_EXCL;
		req.rt.rtm_type = RTN_UNICAST;
	}

	if (from.len) {
		req.rt.rtm_src_len = from.bits;
		addattr_l(&req.nh, sizeof(req), RTA_SRC, &from.data, from.len);
		req.rt.rtm_family=from.family;
	}

	if (to.len) {
		req.rt.rtm_dst_len = to.bits;
		addattr_l(&req.nh, sizeof(req), RTA_DST, &to.data, to.len);
		req.rt.rtm_family=to.family;
	} 

	if (prio)
		addattr32(&req.nh, sizeof(req), RTA_PRIORITY, prio);

	if (dev) {
		addattr_l(&req.nh, sizeof(req), RTA_IIF, dev, strlen(dev)+1);
	} 

	if (req.rt.rtm_family == AF_UNSPEC)
		req.rt.rtm_family = AF_INET;

	if (rtnl_open(&rth, 0) < 0)
		return 1;

	if (rtnl_talk(&rth, &req.nh, 0, 0, NULL, NULL, NULL) < 0)
		return 2;

	return 0;
}
