/* This file is part of Netsukuku
 * (c) Copyright 2006 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
 *
 * This source code is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
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

#include "snsd_cache.h"
#include "snsd.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"

int net_family;

void snsd_init(int family)
{
	net_family=family;
	snsd_cache_init(family);
}


/* 
 * It returns a snsd_service llist (see snsd.h)
 * 
 * In `*records' the number of records stored in the returned snsd_service
 * llist is written.
 * 
 * It returns 0 on error 
 */
snsd_service *snsd_resolve_hname(char *hostname, int service, 
					u_char proto, int *records)
{
	PACKET pkt, rpkt;
	struct andna_resolve_rq_pkt req;
	struct andna_resolve_reply_pkt *reply;
	lcl_cache *lcl;
	rh_cache *rhc;
	andna_cache *ac;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix to;

	snsd_service *sns;
	snsd_prio *snp;
	snsd_node *snd;

	const char *ntop; 
	int ret=0;
	ssize_t err;

	setzero(&req, sizeof(req));
	setzero(&pkt, sizeof(pkt));
	setzero(&rpkt,sizeof(pkt));


	/*
	 * Search the hostname in the local cache first. Maybe we are so
	 * dumb that we are trying to resolve the same ip we registered.
	 */
	if((lcl=lcl_cache_find_hname(andna_lcl, hname)))
		return lcl->service;
	
	/*
	 * Last try before asking to ANDNA: let's see if we have it in the
	 * resolved_hnames cache.
	 */
	if((rhc=rh_cache_find_hname(hname)))
		return rhc->service;
	
	/* 
	 * Fill the request structure.
	 */
	inet_copy_ipdata(req.rip, &me.cur_ip);
	andna_hash(my_family, hname, strlen(hname), req.hash, hash_gnode);
	
	/*
	 * If we manage an andna_cache, it's better to peek at it.
	 */
	if((ac=andna_cache_gethash(req.hash)) &&
			(sns=snsd_find_service(ac->acq->service,
					SNSD_DEFAULT_SERVICE, 
					SNSD_DEFAULT_PROTO)) &&
			(snp=snsd_highest_prio(sns->prio)) &&
			(snd=snsd_choose_wrand(snp->node))) {
		inet_setip_raw(resolved_ip, snd->record, my_family);
		return 0;
	}

	/* 
	 * Ok, we have to ask to someone for the resolution.
	 * Let's see to whom we have to send the pkt.
	 */
	if((err=find_hash_gnode(hash_gnode, &to, 0, 0, 1)) < 0)
		ERROR_FINISH(ret, -1, finish);
	else if(err == 1)
		req.flags|=ANDNA_PKT_FORWARD;
		
	ntop=inet_to_str(to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_RESOLVE_HNAME), ntop);

	
	/* 
	 * Fill the packet and send the request 
	 */
	
	/* host -> network order */
	ints_host_to_network(&req, andna_resolve_rq_pkt_iinfo);
	
	pkt_addto(&pkt, &to);
	pkt.pkt_flags|=PKT_SET_LOWDELAY;
	pkt.hdr.flags|=ASYNC_REPLY;
	pkt.hdr.sz=ANDNA_RESOLVE_RQ_PKT_SZ;
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, &req, pkt.hdr.sz);
	
	setzero(&rpkt, sizeof(PACKET));
	err=send_rq(&pkt, 0, ANDNA_RESOLVE_HNAME, 0, ANDNA_RESOLVE_REPLY, 1, &rpkt);
	if(err==-1) {
		error("andna_resolve_hname(): Resolution of \"%s\" failed.", hname);
		ERROR_FINISH(ret, -1, finish);
	}

	if(rpkt.hdr.sz != ANDNA_RESOLVE_REPLY_PKT_SZ)
		ERROR_FINISH(ret, -1, finish);

	/* 
	 * Take the ip we need from the replied pkt 
	 */
	reply=(struct andna_resolve_reply_pkt *)rpkt.msg;
	inet_setip(resolved_ip, reply->ip, my_family);
	
	/* network -> host order */
	ints_network_to_host(reply, andna_resolve_reply_pkt_iinfo);
	
	/* 
	 * Add the hostname in the resolved_hnames cache since it was
	 * successful resolved it ;)
	 */
	reply->timestamp = time(0) - reply->timestamp;
	rh_cache_add(hname, reply->timestamp, resolved_ip,
			SNSD_DEFAULT_SERVICE,
			SNSD_DEFAULT_PROTO,
			SNSD_DEFAULT_PRIO,
			SNSD_DEFAULT_WEIGHT);
	
finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 0);
	return ret;
	
}

/* 
 * It returns a snsd_service llist (see snsd.h)
 * 
 * In `*records' the number of records stored in the returned snsd_service
 * llist is written.
 * 
 * It returns 0 on error 
 */
snsd_service *snsd_resolve_hash(char hname_hash[ANDNA_HASH_SZ], int *records)
{
}

/* 
 * It returns the local_cache of the node which has the given `ip'.
 * 
 * It returns 0 on error 
 */
lcl_cache *snsd_reverse_resolve(inet_prefix *ip)
{
}
