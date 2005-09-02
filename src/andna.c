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

#include "llist.c"
#include "inet.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "route.h"
#include "request.h"
#include "pkts.h"
#include "tracer.h"
#include "qspn.h"
#include "radar.h"
#include "netsukuku.h"
#include "daemon.h"
#include "crypto.h"
#include "andna_cache.h"
#include "andna.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"


int andna_load_caches(void)
{

	if((andna_lcl=load_lcl_cache(&lcl_keyring, server_opt.lcl_file, &lcl_counter)))
		debug(DBG_NORMAL, "Andna Local Cache loaded");

	if((andna_c=load_andna_cache(server_opt.andna_cache_file, &andna_c_counter)))
		debug(DBG_NORMAL, "Andna cache loaded");

	if((andna_counter_c=load_counter_c(server_opt.counter_c_file, &cc_counter)))
		debug(DBG_NORMAL, "Counter cache loaded");

	if((!load_hostnames(server_opt.andna_hnames_file, &andna_lcl, &lcl_counter)))
		debug(DBG_NORMAL, "Hostnames file loaded");

	return 0;
}

int andna_save_caches(void)
{
	debug(DBG_NORMAL, "Saving the andna local cache");
	save_lcl_cache(&lcl_keyring, andna_lcl, server_opt.lcl_file);

	debug(DBG_NORMAL, "Saving the andna cache");
	save_andna_cache(andna_c, server_opt.andna_cache_file);

	debug(DBG_NORMAL, "Saving the andna counter cache");
	save_counter_c(andna_counter_c, server_opt.counter_c_file);

	return 0;
}

void andna_init(void)
{
	/* register the andna's ops in the pkt_op_table */
	add_pkt_op(ANDNA_REGISTER_HNAME, SKT_TCP, andna_tcp_port, andna_recv_reg_rq);
	add_pkt_op(ANDNA_CHECK_COUNTER,  SKT_TCP, andna_tcp_port, andna_recv_check_counter);
	add_pkt_op(ANDNA_RESOLVE_HNAME,  SKT_UDP, andna_tcp_port, andna_recv_resolve_rq);
	add_pkt_op(ANDNA_RESOLVE_REPLY,  SKT_UDP, andna_tcp_port, 0);
	add_pkt_op(ANDNA_RESOLVE_IP,     SKT_TCP, andna_tcp_port, andna_recv_rev_resolve_rq);
	add_pkt_op(ANDNA_REV_RESOLVE_REPLY,  SKT_TCP, andna_tcp_port, 0);
	add_pkt_op(ANDNA_GET_ANDNA_CACHE,SKT_TCP, andna_tcp_port, put_andna_cache);
	add_pkt_op(ANDNA_PUT_ANDNA_CACHE,SKT_TCP, andna_tcp_port, 0);

	pkt_queue_init();

	andna_caches_init();

	andna_load_caches();
	lcl_new_keyring(&lcl_keyring);
}

/*
 * andna_hash_by_family: If family is equal to AF_INET, in `hash' it stores the
 * 32bit hash of the `msg', otherwise it just copies `msg' to `hash_ip'. 
 * Note that this function is used to hash other hashes, so it operates on the
 * ANDNA_HASH_SZ fixed length.
 */
void andna_hash_by_family(int family, void *msg, int hash[MAX_IP_INT])
{
	memset(hash, 0, ANDNA_HASH_SZ);
	
	if(family==AF_INET)
		hash[0] = fnv_32_buf((u_char *)msg, ANDNA_HASH_SZ, 
				FNV1_32_INIT);
	else
		memcpy(hash, msg, ANDNA_HASH_SZ);
}

/*
 * andna_hash: This functions makes a digest of `msg' which is `len' bytes
 * big and stores it in `hash'. If family is equal to AF_INET, in `ip_hash' it
 * stores the 32bit hash of the `hash', otherwise it just copies `hash' to
 * `hash_ip'.
 */
void andna_hash(int family, void *msg, int len, int hash[MAX_IP_INT],
		int ip_hash[MAX_IP_INT])
{
	hash_md5(msg, len, (u_char *)hash);
	andna_hash_by_family(family, (u_char *)hash, ip_hash);	
}

/*
 * andna_find_hash_gnode: It stores in `to' the ip of the node nearer to the
 * `hash'_gnode and returns 0. If we aren't part of the hash_gnode, it sets in 
 * `to' the ip of the bnode_gnode that will route the pkt and returns 1.
 * If either a hash_gnode and a bnode_gnode aren't found, -1 is returned.
 */
int andna_find_hash_gnode(int hash[MAX_IP_INT], inet_prefix *to)
{
	int level, gid, i, e, steps, total_levels, err;
	quadro_group qg;
	map_gnode *gnode;
	
	total_levels=GET_LEVELS(my_family);

	/* Hash to ip and quadro_group conversion */
	inet_setip(to, hash, my_family);
	iptoquadg(*to, me.ext_map, &qg, QUADG_GID|QUADG_GNODE);
	
	
	/*
	 * This is how the ip nearer to `hash' is found:
	 * - The hash's ip as a quadro_group is stored in `qg'
	 * loop1:
	 * - Examine each level of the hash's ip starting from the highest.
	 *   loop2:
	 * 	- If the gnode `gq'.gid[level] is down increment or decrement 
	 * 	  (alternatively) `gq'.gid[level] and continues the loop2.
	 *	  If it is up, continues in the loop1 if `gq'.gid[level] is a
	 *	  gnode where we belong, otherwise returns the ip of the
	 *	  border_node that will be used to forward the pkt to the
	 *	  hash_gnode. 
	 * - The ip of the rounded hash gnode is in `gq' and it's converted
	 *   back in the inet_prefix format.
	 */
	
	for(level=total_levels-1; level >= 1; level--) {
	
		/* the maximum steps required to complete the second for. */
		steps = qg.gid[level] >= MAXGROUPNODE/2 ? qg.gid[level] : MAXGROUPNODE-qg.gid[level];
		for(i=0, e=1, gid=qg.gid[level]; i < steps; e&1 ? i++ : i, e++) {
			/* `i' is incremented only when `e' is odd, while `e'
			 * is always incremented in each cycle. */

			if(!(e & 1) && (qg.gid[level]+i < MAXGROUPNODE))
				gid=qg.gid[level]+i;
			else if(qg.gid[level]-i >= 0)
				gid=qg.gid[level]-i;
			else 
				continue;

			gnode=gnode_from_pos(gid, me.ext_map[_EL(level)]);
			if(!(gnode->g.flags & MAP_VOID) &&
					!(gnode->flags & GMAP_VOID)) {
				qg.gid[level]=gid;
				if(!quadg_gids_cmp(qg, me.cur_quadg, level))
					break;
				else {
					err=get_gw_ip(me.int_map, me.ext_map, me.bnode_map,
							me.bmap_nodes, &me.cur_quadg,
							gnode, level, 0, to);
					
					return err < 0 ? -1 : 1;
				}
			}
		}
	}

	/* 
	 * Choose the gid of level 0 to complete the hash_gnode ip and be sure
	 * it is up.
	 */
	for(i=0; i<MAXGROUPNODE; i++)
		if(!(me.int_map[i].flags & MAP_VOID) && !(me.int_map[i].flags & MAP_ME)) {
			qg.gid[0]=i;
			gidtoipstart(qg.gid, total_levels, total_levels, my_family, to);
			
			return 0;
		}
	
	return -1;
}

/*
 * andna_register_hname: Register or update the `alcl->hostname' hostname.
 */
int andna_register_hname(lcl_cache *alcl)
{
	PACKET pkt, rpkt;
	struct andna_reg_pkt req;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix to;
	
	char *sign;
	const char *ntop; 
	int ret=0;
	ssize_t err;
	time_t  cur_t;

	memset(&req, 0, sizeof(req));
	memset(&pkt, 0, sizeof(pkt));
	cur_t=time(0);
	
	if(alcl->timestamp) {
		if(cur_t - alcl->timestamp < ANDNA_MIN_UPDATE_TIME)
			/* We have too wait a little more before sending an
			 * update */
			return -1;

		req.flags|=ANDNA_UPDATE;
		req.hname_updates = ++alcl->hname_updates;
	}
	
	/* 
	 * Filling the request structure 
	 */

	memcpy(req.rip, me.cur_ip.data, MAX_IP_SZ); 
	andna_hash(my_family, alcl->hostname, strlen(alcl->hostname),
			req.hash, hash_gnode);
	memcpy(req.pubkey, lcl_keyring.pubkey, ANDNA_PKEY_LEN);
	
	/* Sign the packet */
	sign=rsa_sign((u_char *)&req, ANDNA_REG_SIGNED_BLOCK_SZ, 
			lcl_keyring.priv_rsa, 0);
	memcpy(req.sign, sign, ANDNA_SIGNATURE_LEN);
	
	/* Find the hash_gnode that corresponds to the hash `hash_gnode'*/
	if((err=andna_find_hash_gnode(hash_gnode, &to)) < 0) {
		ret=-1;
		goto finish;
	} else if(err == 1)
		req.flags|=ANDNA_FORWARD;
		
	ntop=inet_to_str(to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_REGISTER_HNAME), ntop);
	
	/* Fill the packet and send the request */
	pkt_addto(&pkt, &to);
	pkt.hdr.flags|=ASYNC_REPLY;
	pkt.hdr.sz=ANDNA_REG_PKT_SZ;
	pkt.msg=xmalloc(ANDNA_REG_PKT_SZ);
	memcpy(pkt.msg, &req, ANDNA_REG_PKT_SZ);
	
	memset(&rpkt, 0, sizeof(PACKET));
	err=send_rq(&pkt, 0, ANDNA_REGISTER_HNAME, 0, ACK_AFFERMATIVE, 1, &rpkt);
	if(err==-1) {
		error("andna_register_hname(): Registration of %s to %s "
				"failed.", alcl->hostname, ntop);
		ret=-1;
		goto finish;
	}

	/* Ehi, we've registered it! Update the hname timestamp */
	alcl->timestamp=cur_t;

finish:
	if(sign)
		xfree(sign);	
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 1);
	return ret;
}

/* 
 * andna_recv_reg_rq: It takes care of a registration request. If we aren't
 * the rightful hash_gnode, we forward the request (again), otherwise the
 * request is examined. If it is valid the hostname is registered or updated
 * in the andna_cache.
 */
int andna_recv_reg_rq(PACKET rpkt)
{
	PACKET pkt;
	struct andna_reg_pkt *req;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix rfrom, to;
	RSA *pubkey;
	andna_cache_queue *acq;
	andna_cache *ac;
	time_t cur_t;
	int ret=0, err;

	req=(struct andna_reg_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_REG_PKT_SZ) {
		ret=-1;
		goto finish;
	}
	
	/* Save the real sender of the request */
	inet_setip(&rfrom, req->rip, my_family);

	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	
	/* Verify the signature */
	pubkey=get_rsa_pub((const u_char **)&req->pubkey, ANDNA_PKEY_LEN);
	if(!verify_sign((u_char *)req, ANDNA_REG_SIGNED_BLOCK_SZ, req->sign, 
				ANDNA_SIGNATURE_LEN, pubkey)) {
		/* Bad, bad signature */
		ret=pkt_err(pkt, E_INVALID_SIGNATURE);
		goto finish;
	}


	andna_hash_by_family(my_family, (u_char *)req->hash, hash_gnode);
	if((err=andna_find_hash_gnode(hash_gnode, &to)) < 0) {
		ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		goto finish;
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			goto finish;
		}

		/* Continue to forward the received pkt */
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Ask the counter_node if it is ok to register/update the hname */
	if(andna_check_counter(rpkt) == -1) {
		ret=pkt_err(pkt, E_ANDNA_CHECK_COUNTER);
		goto finish;
	}

	/* Finally, let's register/update the hname */
	ac=andna_cache_addhash(req->hash);
	acq=ac_queue_add(ac, rfrom, req->pubkey);
	if(!acq) {
		ret=pkt_err(pkt, E_ANDNA_QUEUE_FULL);
		goto finish;
	}

	if(acq->hname_updates != req->hname_updates) {
		ret=pkt_err(pkt, E_INVALID_REQUEST);
		goto finish;
	} else
		acq->hname_updates++;

	cur_t=time(0);	
	if((cur_t - acq->timestamp) < ANDNA_MIN_UPDATE_TIME) {
		ret=pkt_err(pkt, E_ANDNA_UPDATE_TOO_EARLY);
		goto finish;
	}
	
	/* Reply to the requester: <<Yes, don't worry, it worked.>> */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK) {
		pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id,
				ACK_AFFERMATIVE, 0);
		pkt.msg=0;
		ret=forward_pkt(pkt, rfrom);
	}
	
	/* 
	 * Broadcast the request to the entire gnode of level 1 to let the
	 * other nodes register the hname.
	 */
	rpkt.sk=0;
	/* be sure that the other nodes don't reply to rfrom again */
	rpkt.hdr.flags&=~SEND_ACK & ~ASYNC_REPLY;
	flood_pkt_send(exclude_glevel, 1, -1, -1, rpkt);

finish:
	pkt_free(&pkt, 1);
	return ret;
}

/*
 * andna_check_counter: asks to the counter_node if it is ok to register the
 * hnmae present in the register request in `pkt'.
 * If -1 is returned the answer is no.
 */
int andna_check_counter(PACKET pkt)
{
	PACKET rpkt;
	int ret=0, pubk_hash[MAX_IP_INT], hash_gnode[MAX_IP_INT], err;
	struct andna_reg_pkt *req;
	const char *ntop; 

	/* Calculate the hash of the pubkey of the sender node. This hash will
	 * be used to reach its counter node. */
	req=(struct andna_reg_pkt *)pkt.msg;
	andna_hash(my_family, req->pubkey, ANDNA_PKEY_LEN, pubk_hash, hash_gnode);
	
	/* Find a hash_gnode for the pubk_hash */
	req->flags&=~ANDNA_FORWARD;
	if((err=andna_find_hash_gnode(hash_gnode, &pkt.to)) < 0) {
		ret=-1;
		goto finish;
	} else if(err == 1)
		req->flags|=ANDNA_FORWARD;
	
	ntop=inet_to_str(pkt.to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_CHECK_COUNTER), ntop);
	
	pkt.hdr.flags|=ASYNC_REPLY;
	
	/* Append our ip in the pkt */
	pkt.hdr.sz+=MAX_IP_SZ;
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, req, ANDNA_REG_PKT_SZ);
	memcpy(pkt.msg+ANDNA_REG_PKT_SZ, me.cur_ip.data, MAX_IP_SZ);
	
	/* Throw it */
	memset(&rpkt, 0, sizeof(PACKET));
	ret=send_rq(&pkt, 0, ANDNA_CHECK_COUNTER, 0, ACK_AFFERMATIVE, 1, &rpkt);

finish:
	return ret;
}

int andna_recv_check_counter(PACKET rpkt)
{
	PACKET pkt;
	struct andna_reg_pkt *req;
	inet_prefix rfrom, to;
	RSA *pubkey;
	counter_c *cc;
	counter_c_hashes *cch;
	int ret=0, pubk_hash[MAX_IP_INT], hash_gnode[MAX_IP_INT], err;

	req=(struct andna_reg_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_REG_PKT_SZ+MAX_IP_SZ) {
		ret=-1;
		goto finish;
	}
	
	/* Save the real sender of the request */
	inet_setip(&rfrom, (u_int *)rpkt.msg+ANDNA_REG_PKT_SZ, my_family);

	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	
	/* Verify the signature */
	pubkey=get_rsa_pub((const u_char **)&req->pubkey, ANDNA_PKEY_LEN);
	if(!verify_sign((u_char *)req, ANDNA_REG_SIGNED_BLOCK_SZ, req->sign, 
				ANDNA_SIGNATURE_LEN, pubkey)) {
		/* Bad signature */
		ret=pkt_err(pkt, E_INVALID_SIGNATURE);
		goto finish;
	}

	/* 
	 * Check if we are the real counter node or if we have to continue to 
	 * forward the pkt 
	 */
	andna_hash(my_family, req->pubkey, ANDNA_PKEY_LEN, pubk_hash, hash_gnode);
	if((err=andna_find_hash_gnode(hash_gnode, &to)) < 0) {
		ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		goto finish;
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			goto finish;
		}

		/* Continue to forward the received pkt */
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Finally, let's register/update the hname */
	cc=counter_c_add(&rfrom, req->pubkey);
	cch=cc_hashes_add(cc, req->hash);
	if(!cch) {
		ret=pkt_err(pkt, E_ANDNA_TOO_MANY_HNAME);
		goto finish;
	}

	if(cch->hname_updates != req->hname_updates) {
		ret=pkt_err(pkt, E_INVALID_REQUEST);
		goto finish;
	} else
		cch->hname_updates++;
		
	/* Report the successful result to rfrom */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK) {
		pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id, ACK_AFFERMATIVE, 0);
		pkt.msg=0;
		ret=forward_pkt(pkt, rfrom);
	}
	
	/* 
	 * Broadcast the request to the entire gnode of level 1 to let the
	 * other counter_nodes register the hname.
	 */
	rpkt.sk=0;
	/* be sure that the other nodes don't reply to rfrom again */
	rpkt.hdr.flags&=~SEND_ACK & ~ASYNC_REPLY;
	flood_pkt_send(exclude_glevel, 1, -1, -1, rpkt);

finish:
	pkt_free(&pkt, 1);
	return ret;

}

/*
 * andna_resolve_hname: stores in `resolved_ip' the ip associated to the
 * `hname' hostname.
 * On error -1 is returned.
 */
int andna_resolve_hname(char *hname, inet_prefix *resolved_ip)
{
	PACKET pkt, rpkt;
	struct andna_resolve_rq_pkt req;
	struct andna_resolve_reply_pkt *reply;
	lcl_cache *lcl;
	rh_cache *rhc;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix to;
	
	const char *ntop; 
	int ret=0;
	ssize_t err;

	memset(&req, 0, sizeof(req));
	memset(&pkt, 0, sizeof(pkt));

	/*
	 * Search in the hostname in the local cache first. Maybe we are so
	 * dumb that we are trying to resolve the same ip we registered.
	 */
	if((lcl=lcl_cache_find_hname(andna_lcl, hname))) {
		memcpy(resolved_ip, &me.cur_ip, sizeof(inet_prefix));
		return ret;
	}
	
	/*
	 * Last try before going asking to ANDNA: let's if we have it in the
	 * resolved_hnames cache
	 */
	if((rhc=rh_cache_find_hname(hname))) {
		memcpy(resolved_ip, &rhc->ip, sizeof(inet_prefix));
		return ret;
	}
	
	/* 
	 * Ok, we have to ask to someone other.
	 * Fill the request structure.
	 */
	memcpy(req.rip, me.cur_ip.data, MAX_IP_SZ);
	andna_hash(my_family, hname, strlen(hname), req.hash, hash_gnode);
	
	/* Let's see to whom we have to send the pkt */
	if((err=andna_find_hash_gnode(hash_gnode, &to)) < 0) {
		ret=-1;
		goto finish;
	} else if(err == 1)
		req.flags|=ANDNA_FORWARD;
		
	ntop=inet_to_str(to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_REGISTER_HNAME), ntop);
	
	/* Fill the packet and send the request */
	pkt_addto(&pkt, &to);
	pkt.hdr.flags|=ASYNC_REPLY;
	pkt.hdr.sz=ANDNA_RESOLVE_RQ_PKT_SZ;
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, &req, pkt.hdr.sz);
	
	memset(&rpkt, 0, sizeof(PACKET));
	err=send_rq(&pkt, 0, ANDNA_RESOLVE_HNAME, 0, ANDNA_RESOLVE_REPLY, 1, &rpkt);
	if(err==-1) {
		error("andna_resolve_hname(): Resolution of \"%s\" failed.", hname);
		ret=-1;
		goto finish;
	}

	if(rpkt.hdr.sz != ANDNA_RESOLVE_REPLY_PKT_SZ) {
		ret=-1;
		goto finish;
	}
		
	reply=(struct andna_resolve_reply_pkt *)rpkt.msg;
	inet_setip(resolved_ip, reply->ip, my_family);
	
	/* 
	 * Add the hostname in the resolved_hnames cache since it was
	 * successful resolved it ;)
	 */
	rh_cache_add(hname, reply->timestamp, resolved_ip);
	
finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 1);
	return ret;
}

/*
 * andna_recv_resolve_rq: replies to a hostname resolve request by giving the
 * ip associated to the hostname.
 */
int andna_recv_resolve_rq(PACKET rpkt)
{
	PACKET pkt;
	struct andna_resolve_rq_pkt *req;
	struct andna_resolve_reply_pkt reply;
	andna_cache *ac;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix rfrom, to;
	int ret=0, err;

	req=(struct andna_resolve_rq_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_RESOLVE_RQ_PKT_SZ) {
		ret=-1;
		goto finish;
	}
	
	/* Save the real sender of the request */
	inet_setip(&rfrom, req->rip, my_family);

	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));

	/*
	 * Are we the right hash_gnode or have we to still forward the pkt ?
	 */
	andna_hash_by_family(my_family, (u_char *)req->hash, hash_gnode);
	if((err=andna_find_hash_gnode(hash_gnode, &to)) < 0) {
		ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		goto finish;
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			goto finish;
		}

		/* Continue to forward the received pkt */
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Search the hostname to resolve in the andna_cache */
	andna_cache_del_expired();
	if(!(ac=andna_cache_findhash(req->hash))) {
		/* There isn't it, bye. */
		ret=pkt_err(pkt, E_ANDNA_NO_HNAME);
		goto finish;
	}
	
	/* Send back the ip associated to the hname */
	memset(&reply, 0, sizeof(reply));
	memcpy(reply.ip, ac->acq->rip.data, MAX_IP_SZ);
	reply.timestamp=ac->acq->timestamp;
	pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id, ANDNA_RESOLVE_REPLY,
			sizeof(reply));
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, &reply, sizeof(reply));
	ret=forward_pkt(pkt, rfrom);
	
finish:
	pkt_free(&pkt, 1);
	return ret;
}

/*
 * andna_reverse_resolve: it sends to `ip' a reverse resolve request to
 * receive all the hostnames that `ip' has registered.
 * It returns the number of hostnames that are stored in `hostnames'.
 * If there aren't any hostnames or an error occurred, -1 is returned.
 */
int andna_reverse_resolve(inet_prefix ip, char ***hostnames)
{
	PACKET pkt, rpkt;
	inet_prefix to;
	struct andna_rev_resolve_reply_hdr *reply;
	
	const char *ntop; 
	int ret=0, tot_hnames, i, sz;
	u_short *hnames_sz;
	char **hnames, *buf;
	ssize_t err;

	memset(&pkt, 0, sizeof(pkt));
	memset(&rpkt, 0, sizeof(PACKET));
	memcpy(&to, &ip, sizeof(inet_prefix));
	
	ntop=inet_to_str(to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_REGISTER_HNAME), ntop);
	
	/* Fill the packet and send the request */
	pkt_addto(&pkt, &to);
	pkt_addfrom(&rpkt, &to);
	err=send_rq(&pkt, 0, ANDNA_RESOLVE_IP, 0, ANDNA_REV_RESOLVE_REPLY, 1, &rpkt);
	if(err==-1) {
		error("andna_reverse_resolve(): Reverse resolution of the %s "
				"ip failed.", inet_to_str(to));
		ret=-1;
		goto finish;
	}

	reply=(struct andna_rev_resolve_reply_hdr *)rpkt.msg;
	tot_hnames=reply->hostnames;
	tot_hnames++;
	if(tot_hnames > ANDNA_MAX_HOSTNAMES || tot_hnames <= 0) {
		ret=-1;
		goto finish;
	}

	/* 
	 * Split the received hostnames 
	 */
	hnames_sz=(u_short *)((char *)rpkt.msg+sizeof(struct andna_rev_resolve_reply_hdr));
	hnames=xmalloc(tot_hnames * sizeof(char *));
	
	sz=sizeof(struct andna_rev_resolve_reply_hdr)+sizeof(u_short)*tot_hnames;
	buf=rpkt.msg+sz;
	for(i=0; i<tot_hnames; i++) {
		sz+=hnames_sz[i];
		
		if(sz > rpkt.hdr.sz)
			break;
		
		if(hnames_sz[i] > ANDNA_MAX_HNAME_LEN)
			continue;
		
		hnames[i]=xmalloc(hnames_sz[i]);
		memcpy(hnames[i], buf, hnames_sz[i]);
		hnames[i][hnames_sz[i]]=0;

		buf+=hnames_sz[i];
	}

	ret=tot_hnames;
	*hostnames=hnames;
finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 1);
	return ret;
}

/*
 * andna_recv__rev_resolve_rq: it replies to a reverse hostname resolve request
 * which asks all the hostnames associated with a given ip.
 */
int andna_recv_rev_resolve_rq(PACKET rpkt)
{
	PACKET pkt;
	struct andna_rev_resolve_reply_hdr hdr;
	u_short *hnames_sz;
	char *buf;
	int i, ret=0, err;
	
	lcl_cache *alcl=andna_lcl;

	memset(&pkt, 0, sizeof(PACKET));
	
	/*
	 * Build the reply pkt
	 */
	
	pkt_fill_hdr(&pkt.hdr, 0, rpkt.hdr.id, ANDNA_RESOLVE_REPLY, 0);
	pkt.hdr.sz=sizeof(struct andna_rev_resolve_reply_hdr)+sizeof(u_short);
	
	hdr.hostnames=lcl_counter;
	if(hdr.hostnames) {
		hnames_sz=xmalloc(sizeof(u_short) * hdr.hostnames);
		i=0;
		list_for(alcl) {
			hnames_sz[i++]=strlen(alcl->hostname);
			pkt.hdr.sz+=hnames_sz[i];
		}
	}

	/* 
	 * Pack all the hostnames we have (if any) 
	 */

	pkt.msg=buf=xmalloc(pkt.hdr.sz);
	memcpy(buf, &hdr, sizeof(struct andna_rev_resolve_reply_hdr));
	
	if(hdr.hostnames) {
		buf+=sizeof(struct andna_rev_resolve_reply_hdr);
		memcpy(buf, hnames_sz, sizeof(u_short) * hdr.hostnames);

		buf+=sizeof(u_short) * hdr.hostnames;
		
		i=0;
		alcl=andna_lcl;
		list_for(alcl) {
			memcpy(buf, alcl->hostname, hnames_sz[i++]);
			buf+=hnames_sz[i];
		}
	}

	/*
	 * Send it.
	 */

        pkt_addto(&pkt, &rpkt.from);
	pkt_addsk(&pkt, my_family, rpkt.sk, rpkt.sk_type);
        err=send_rq(&pkt, 0, ANDNA_REV_RESOLVE_REPLY, rpkt.hdr.id, 0, 0, 0);
        if(err==-1) {
                ret=-1;
                goto finish;
        }
finish:
	pkt_free(&pkt, 1);
	return ret;
}

/*
 * get_andna_cache: sends the ANDNA_GET_ANDNA_CACHE request to `to' to retrieve the
 * andna_cache from `to'.
 */
andna_cache *get_andna_cache(inet_prefix to, int *counter)
{
	PACKET pkt, rpkt;
	andna_cache *andna_cache, *ret=0;
	size_t pack_sz;
	int err;
	const char *ntop;
	char *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_GET_ANDNA_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_GET_ANDNA_CACHE, 0, ANDNA_PUT_ANDNA_CACHE, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	pack_sz=rpkt.hdr.sz;
	pack=rpkt.msg;
	ret=andna_cache=unpack_andna_cache(pack, pack_sz, counter);
	if(!andna_cache)
		error("get_andna_cache(): Malformed andna_cache. Cannot load it");
	
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
	return ret;
}

/*
 * put_andna_cache: replies to a ANDNA_GET_ANDNA_CACHE request, sending back the
 * complete andna cache
 */
int put_andna_cache(PACKET rq_pkt)
{
	PACKET pkt;
	const char *ntop; 
	int ret;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the ANDNA_PUT_ANDNA_CACHE reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_andna_cache(andna_c, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(ANDNA_PUT_ANDNA_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_PUT_ANDNA_CACHE, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_andna_cache(): Cannot send the ANDNA_PUT_ANDNA_CACHE reply to %s.", ntop);
		ret=-1;
		goto finish;
	}
finish:
	pkt_free(&pkt, 0);
	return ret;
}


/*
 * get_counter_cache: sends the ANDNA_GET_COUNT_CACHE request to `to' to retrieve the
 * counter_cache from `to'.
 */
counter_c *get_counter_cache(inet_prefix to, int *counter)
{
	PACKET pkt, rpkt;
	counter_c *ccache, *ret=0;
	size_t pack_sz;
	int err;
	const char *ntop;
	char *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_GET_COUNT_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_GET_COUNT_CACHE, 0, ANDNA_PUT_COUNT_CACHE, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	pack_sz=rpkt.hdr.sz;
	pack=rpkt.msg;
	ret=ccache=unpack_counter_cache(pack, pack_sz, counter);
	if(!ccache)
		error("get_counter_cache(): Malformed counter_cache. Cannot load it");
	
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
	return ret;
}

/*
 * put_counter_cache: replies to a ANDNA_GET_COUNT_CACHE request, sending back the
 * complete andna cache.
 */
int put_counter_cache(PACKET rq_pkt)
{
	PACKET pkt;
	const char *ntop; 
	int ret;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the ANDNA_PUT_COUNT_CACHE reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_counter_cache(andna_counter_c, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(ANDNA_PUT_COUNT_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_PUT_COUNT_CACHE, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_counter_cache(): Cannot send the ANDNA_PUT_COUNT_CACHE reply to %s.", ntop);
		ret=-1;
		goto finish;
	}
finish:
	pkt_free(&pkt, 0);
	return ret;
}

/*
 * andna_hook: The andna_hook gets the andna_cache and the counter_node cache
 * from the nearest rnodes.
 */
void *andna_hook(void *null)
{
	inet_prefix to;
	map_node *node;
	int e=0, i;
	
	memset(&to, 0, sizeof(inet_prefix));

	/* 
	 * Send the GET_ANDNA_CACHE request to the nearest rnode we have, if it
	 * fails send it to the second rnode and so on...
	 */
	for(i=0, e=0; i < me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;
		if(!node || node->flags & MAP_ERNODE)
			continue;

		/* We need the ip of the rnode ;^ */
		rnodetoip((u_int)me.int_map, (u_int)node, 
				me.cur_quadg.ipstart[1], &to);
		
		andna_c=get_andna_cache(to, &andna_c_counter);
		if(andna_c) {
			e=1;
			break;
		}
	}
	if(!e)
		loginfo("None of the rnodes in this area gave me the andna_cache.");

	/* 
	 * Send the GET_COUNT_CACHE request to the nearest rnode we have, if it
	 * fails send it to the second rnode and so on...
	 */
	for(i=0, e=0; i < me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;
		if(!node || node->flags & MAP_ERNODE)
			continue;
		
		/* We need the ip of the rnode ;^ */
		rnodetoip((u_int)me.int_map, (u_int)node, 
				me.cur_quadg.ipstart[1], &to);
		
		andna_counter_c=get_counter_cache(to, &cc_counter);
		if(andna_counter_c) {
			e=1;
			break;
		}
	}
	if(!e)
		loginfo("None of the rnodes in this area gave me the counter_cache.");

	return 0;
}

/*
 * andna_register_new_hnames: registers the newly hostnames added in the local
 * cache (usually by load_hostnames()).
 */
void andna_register_new_hnames(void)
{
	lcl_cache *alcl=andna_lcl;
	int ret;

	list_for(alcl) {
		if(alcl->timestamp)
			continue;
		ret=andna_register_hname(alcl);
		if(!ret)
			loginfo("hostnames \"%s\" registered/updated "
					"successfully", alcl->hostname);
	}
}

/*
 * andna_maintain_hnames_active: periodically registers and keep up to date the
 * hostnames of the local cache.
 */
void *andna_maintain_hnames_active(void *null)
{
	lcl_cache *alcl=andna_lcl;
	int ret;

	for(;;) {
		list_for(alcl) {
			ret=andna_register_hname(alcl);
			if(!ret)
				loginfo("hostnames \"%s\" registered/updated "
						"successfully", alcl->hostname);
		}
		sleep((ANDNA_EXPIRATION_TIME/2) + rand_range(1, 20));
	}

	return 0;
}

int andna_main(void)
{
	u_short *port;
	pthread_t thread;
	pthread_attr_t t_attr;
	
	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

	port=xmalloc(sizeof(u_short));

	debug(DBG_SOFT,   "Evocating the andna udp daemon.");
	*port=andna_udp_port;
	pthread_create(&thread, &t_attr, udp_daemon, (void *)port);

	debug(DBG_SOFT,   "Evocating the andna tcp daemon.");
	*port=andna_tcp_port;
	pthread_create(&thread, &t_attr, tcp_daemon, (void *)port);

	/* Start the hostnames updater and register */
	pthread_create(&thread, &t_attr, andna_maintain_hnames_active, 0);
	
	/* Start the ANDNA hook */
	pthread_create(&thread, &t_attr, andna_hook, 0);

	xfree(port);
	return 0;
}
