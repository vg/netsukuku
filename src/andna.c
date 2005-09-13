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
 *
 * --
 * andna.c:
 * Here there are all the functions that send, receive and exec ANDNA packets.
 * All the threads of the ANDNA daemon and the main andna functions are here 
 * too.
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

	if((andna_rhc=load_rh_cache(server_opt.rhc_file, &rhc_counter)))
		debug(DBG_NORMAL, "Resolved hostnames cache loaded");

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

	debug(DBG_NORMAL, "Saving the resolved hnames cache");
	save_rh_cache(andna_rhc, server_opt.rhc_file);

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
	add_pkt_op(ANDNA_GET_COUNT_CACHE,SKT_TCP, andna_tcp_port, put_counter_cache);
	add_pkt_op(ANDNA_PUT_COUNT_CACHE,SKT_TCP, andna_tcp_port, 0);

	pkt_queue_init();

	andna_caches_init();

	andna_load_caches();
	lcl_new_keyring(&lcl_keyring);

	memset(last_reg_pkt_id, 0, sizeof(int)*ANDNA_MAX_FLOODS);
	memset(last_counter_pkt_id, 0, sizeof(int)*ANDNA_MAX_FLOODS);
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
 * is_hgnode_excluded: it converts the `qg'->gid to an ip, then it searches a
 * member of `excluded_hgnode' which is equal to the ip. If it is found 1 is
 * returned, otherwise 0 is the return value.
 * This function is utilised by find_hash_gnode() to exclude from the search
 * the hash_gnodes not wanted.
 */
int is_hgnode_excluded(quadro_group *qg, u_int *excluded_hgnode[MAX_IP_INT],
		int tot_excluded_hgnodes)
{
	inet_prefix qip;
	int i, total_levels=GET_LEVELS(my_family);
	
	gidtoipstart(qg->gid, total_levels, total_levels, my_family, &qip);
	
	for(i=0; i<tot_excluded_hgnodes; i++)
		if(!memcmp(&excluded_hgnode[i][1], &qip.data[1], 
					MAX_IP_SZ-sizeof(int)))
			return 1;
	return 0;
}

/*
 * find_hash_gnode: It stores in `to' the ip of the node nearer to the
 * `hash'_gnode and returns 0. If we aren't part of the hash_gnode, it sets in 
 * `to' the ip of the bnode_gnode that will route the pkt and returns 1.
 * If either a hash_gnode and a bnode_gnode aren't found, -1 is returned.
 * All the hash_gnodes included in `excluded_hgnode' will be excluded by the
 * algorithm.
 * If `exclude_me' is set to 1, it will not return ourself as the hash_gnode.
 */
int find_hash_gnode(u_int hash[MAX_IP_INT], inet_prefix *to, 
	u_int *excluded_hgnode[MAX_IP_INT], int tot_excluded_hgnodes,
	int exclude_me)
{
	int level, gid, i, e, steps, total_levels, err, x;
	quadro_group qg;
	map_gnode *gnode;
	
	total_levels=GET_LEVELS(my_family);

	/* Hash to ip and quadro_group conversion */
	inet_setip(to, hash, my_family);
	inet_htonl(to);
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
	
		debug(DBG_NOISE, "find_hashgnode: lvl %d, start gid %d", level, qg.gid[level]);
		
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
			
				/* Is this hash_gnode not wanted ? */
				if(excluded_hgnode &&
					is_hgnode_excluded(qg, excluded_hgnode, tot_excluded_hgnodes))
					continue; /* yea */
							
				if(!quadg_gids_cmp(qg, me.cur_quadg, level)) 
					break;
				else {
					err=get_gw_ip(me.int_map, me.ext_map, me.bnode_map,
							me.bmap_nodes, &me.cur_quadg,
							gnode, level, 0, to);
					debug(DBG_NOISE, "find_hashgnode: ext_found, err %d, to %s!",
							err, inet_to_str(*to));
					
					return err < 0 ? -1 : 1;
				}
			}
		}
	}

	/* 
	 * Choose a random gid of level 0 to complete the hash_gnode ip and be
	 * sure it is up.
	 */
	for(x=0, e=i=rand_range(0, MAXGROUPNODE-1); e<MAXGROUPNODE; e++) {
		if(!(me.int_map[e].flags & MAP_VOID)) {
			if(exclude_me && (me.int_map[e].flags & MAP_ME))
				continue;
			qg.gid[0]=e;
			x=1;
			break;
		}
	}
	if(!x)
		for(x=0; i>=0; i--) {
			if(!(me.int_map[i].flags & MAP_VOID)) {
				if(exclude_me && (me.int_map[i].flags & MAP_ME))
					continue;
				qg.gid[0]=i;
				x=1;
				break;
			}
		}
	if(x) {
		gidtoipstart(qg.gid, total_levels, total_levels, my_family, to);
		debug(DBG_NOISE, "find_hashgnode: Internal found: gid0 %d, to %s!",
				qg.gid[0], inet_to_str(*to));
		return 0;
	}


	return -1;
}

/* 
 * andna_find_flood_pkt_id: Search in the `ids_array' a member which is equal
 * to `pkt_id', if it is found its array position is returned otherwise -1
 * will be the return value
 */
int andna_find_flood_pkt_id(int *ids_array, int pkt_id)
{
	int i;
	
	for(i=0; i < ANDNA_MAX_FLOODS; i++)
		if(ids_array[i] == pkt_id)
			return i;
	return -1;
}

/*
 * andna_add_flood_pkt_id: If the `pkt_id' is already present in the
 * `ids_array', 1 is returned, otherwise `pkt_id' is added at the 0 position
 * of the array. All the array elements, except the last one, will be
 * preserved and shifted of one position like a FIFO.
 */
int andna_add_flood_pkt_id(int *ids_array, int pkt_id)
{
	int i;

	if((i=andna_find_flood_pkt_id(ids_array, pkt_id)) < 0) {
		int tmp_array[ANDNA_MAX_FLOODS-1];
		
		/* Shift the array of one position to free the position 0
		 * where the new id will be added */
		memcpy(tmp_array, ids_array, sizeof(int)*(ANDNA_MAX_FLOODS-1));
		memcpy(&ids_array[1], tmp_array, sizeof(int)*(ANDNA_MAX_FLOODS-1));
		ids_array[0]=pkt_id;

		return 0;
	}

	return 1;
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
	memset(&rpkt, 0, sizeof(PACKET));
	cur_t=time(0);

	if(alcl->flags & ANDNA_UPDATING) 
		/* we are already updating this node! */
		return 0;

	alcl->flags|=ANDNA_UPDATING;
	
	if(alcl->timestamp) {
		if(cur_t > alcl->timestamp && 
			(cur_t - alcl->timestamp) < ANDNA_MIN_UPDATE_TIME)
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
	if((err=find_hash_gnode(hash_gnode, &to, 1)) < 0) {
		debug(DBG_SOFT, "andna_register_hname: hash_gnode not found ;(");
		ERROR_FINISH(ret, -1, finish);
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
	
	err=send_rq(&pkt, 0, ANDNA_REGISTER_HNAME, 0, ACK_AFFERMATIVE, 1, &rpkt);
	if(err==-1) {
		error("andna_register_hname(): Registration of \"%s\" to %s "
				"failed.", alcl->hostname, ntop);
		ERROR_FINISH(ret, -1, finish);
	}

	/* Ehi, we've registered it! Update the hname timestamp */
	alcl->timestamp=cur_t;

finish:
	alcl->flags&=~ANDNA_UPDATING;
	if(sign)
		xfree(sign);	
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 0);
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
	PACKET pkt, flood_pkt;
	struct andna_reg_pkt *req;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix rfrom, to;
	RSA *pubkey;
	andna_cache_queue *acq;
	andna_cache *ac;
	time_t cur_t;
	int ret=0, err, real_from_rpos;
	char *ntop=0, *rfrom_ntop=0;
	const u_char *pk;
	int(*exclude_function)(TRACER_PKT_EXCLUDE_VARS);


	req=(struct andna_reg_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_REG_PKT_SZ)
		ERROR_FINISH(ret, -1, finish);

	/* Check if we already received this pkt during the flood */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK &&
			andna_add_flood_pkt_id(last_reg_pkt_id, rpkt.hdr.id)) {
		debug(DBG_INSANE, "Dropped 0x%0x andna pkt, we already "
				"received it", rpkt.hdr.id);
		ERROR_FINISH(ret, 0, finish);
	}

	/* Save the real sender of the request */
	inet_setip(&rfrom, req->rip, my_family);
	inet_htonl(&rfrom);

	ntop=xstrdup(inet_to_str(rpkt.from));
	rfrom_ntop=xstrdup(inet_to_str(rfrom));
	debug(DBG_SOFT, "Andna Registration request 0x%x from: %s, "
			"real from: %s", rpkt.hdr.id, ntop, rfrom_ntop);

	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	/* Send the replies in UDP, they are so tiny */
	pkt_addsk(&pkt, my_family, 0, SKT_UDP);
	
	/* Verify the signature */
	pk=req->pubkey;
	pubkey=get_rsa_pub(&pk, ANDNA_PKEY_LEN);
	if(!pubkey || !verify_sign((u_char *)req, ANDNA_REG_SIGNED_BLOCK_SZ,
				req->sign, ANDNA_SIGNATURE_LEN, pubkey)) {
		/* Bad, bad signature */
		debug(DBG_SOFT, "Invalid signature of the 0x%x reg request", 
				rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_INVALID_SIGNATURE);
		ERROR_FINISH(ret, -1, finish);
	}


	andna_hash_by_family(my_family, (u_char *)req->hash, hash_gnode);
	if((err=find_hash_gnode(hash_gnode, &to, 0)) < 0) {
		debug(DBG_SOFT, "We are not the right (rounded)hash_gnode. "
				"Rejecting the 0x%x reg request", rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		ERROR_FINISH(ret, -1, finish);
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
				ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			ERROR_FINISH(ret, -1, finish);
		}

		/* Continue to forward the received pkt */
		debug(DBG_SOFT, "The reg request pkt will be forwarded to: %s",
				inet_to_str(to));
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Ask the counter_node if it is ok to register/update the hname */
	if(andna_check_counter(rpkt) == -1) {
		debug(DBG_SOFT, "Registration rq 0x%x rejected: %s", 
				rpkt.hdr.id, rq_strerror(E_ANDNA_CHECK_COUNTER));
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_CHECK_COUNTER);
		ERROR_FINISH(ret, -1, finish);
	}

	/* Finally, let's register/update the hname */
	ac=andna_cache_addhash(req->hash);
	acq=ac_queue_add(ac, rfrom, req->pubkey);
	if(!acq) {
		debug(DBG_SOFT, "Registration rq 0x%x rejected: %s", 
				rpkt.hdr.id, rq_strerror(E_ANDNA_QUEUE_FULL));
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_QUEUE_FULL);
		ERROR_FINISH(ret, -1, finish);
	}

	if(acq->hname_updates > req->hname_updates) {
		debug(DBG_SOFT, "Registration rq 0x%x rejected: hname_updates"
				" mismatch", rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_INVALID_REQUEST);
		ERROR_FINISH(ret, -1, finish);
	} else
		acq->hname_updates=req->hname_updates+1;

	cur_t=time(0);	
	if(cur_t > acq->timestamp && 
			(cur_t - acq->timestamp) < ANDNA_MIN_UPDATE_TIME) {
		debug(DBG_SOFT, "Registration rq 0x%x rejected: %s", 
			rpkt.hdr.id, rq_strerror(E_ANDNA_UPDATE_TOO_EARLY));
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_UPDATE_TOO_EARLY);
		ERROR_FINISH(ret, -1, finish);
	}
	
	/* Reply to the requester: <<Yes, don't worry, it worked.>> */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK) {
		debug(DBG_SOFT, "Registration rq 0x%x accepted.", rpkt.hdr.id);
		pkt.msg=0;
		pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id, ACK_AFFERMATIVE, 0);
		ret=forward_pkt(pkt, rfrom);
	}
	
	/* 
	 * Broadcast the request to the entire gnode of level 1 to let the
	 * other nodes register the hname.
	 */
	
	pkt_copy(&flood_pkt, &rpkt);
	flood_pkt.sk=0;
	/* be sure that the other nodes don't reply to rfrom again */
	flood_pkt.hdr.flags&=~SEND_ACK & ~ASYNC_REPLY;

	/* If the pkt was sent from an our rnode, ignore it while flooding */
	real_from_rpos=ip_to_rfrom(flood_pkt.from, 0, 0, 0);
	if(real_from_rpos < 0) {
		exclude_function=exclude_glevel;
		real_from_rpos=-1;
	} else
		exclude_function=exclude_from_and_glevel;

	flood_pkt_send(exclude_function, 1, -1, real_from_rpos, flood_pkt);

finish:
	if(ntop)
		xfree(ntop);
	if(rfrom_ntop)
		xfree(rfrom_ntop);
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
	if((err=find_hash_gnode(hash_gnode, &pkt.to, 0)) < 0)
		ERROR_FINISH(ret, -1, finish);
	else if(err == 1)
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
	pkt.sk=0;  /* Create a new connection */
	ret=send_rq(&pkt, 0, ANDNA_CHECK_COUNTER, 0, ACK_AFFERMATIVE, 1, &rpkt);

finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 0);
	return ret;
}

int andna_recv_check_counter(PACKET rpkt)
{
	PACKET pkt, flood_pkt;
	struct andna_reg_pkt *req;
	inet_prefix rfrom, to;
	RSA *pubkey;
	counter_c *cc;
	counter_c_hashes *cch;
	const u_char *pk;
	char *ntop=0, *rfrom_ntop=0, *buf;
	int ret=0, pubk_hash[MAX_IP_INT], hash_gnode[MAX_IP_INT], err;
	int real_from_rpos;
	int(*exclude_function)(TRACER_PKT_EXCLUDE_VARS);

			
	ntop=xstrdup(inet_to_str(rpkt.from));
	
	req=(struct andna_reg_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_REG_PKT_SZ+MAX_IP_SZ) {
		debug(DBG_SOFT, "Malformed check_counter pkt from %s", ntop);
		ERROR_FINISH(ret, -1, finish);
	}
	
	/* Check if we already received this pkt during the flood */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK &&
			andna_add_flood_pkt_id(last_counter_pkt_id, rpkt.hdr.id)) {
		debug(DBG_INSANE, "Dropped 0x%0x andna pkt, we already "
				"received it", rpkt.hdr.id);
		ERROR_FINISH(ret, 0, finish);
	}

	/* Save the real sender of the request */
	buf=rpkt.msg+ANDNA_REG_PKT_SZ;
	inet_setip(&rfrom, (u_int *)buf, my_family);
	inet_htonl(&rfrom);

	rfrom_ntop=xstrdup(inet_to_str(rfrom));
	debug(DBG_SOFT, "Received %s from %s, rfrom %s", rq_to_str(rpkt.hdr.op),
			ntop, rfrom_ntop);
	
	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	/* Reply to rfrom using a UDP sk, since the replies are very small */
	pkt_addsk(&pkt, my_family, 0, SKT_UDP);
	
	/* Verify the signature */
	pk=req->pubkey;
	pubkey=get_rsa_pub(&pk, ANDNA_PKEY_LEN);
	if(!verify_sign((u_char *)req, ANDNA_REG_SIGNED_BLOCK_SZ, req->sign, 
				ANDNA_SIGNATURE_LEN, pubkey)) {
		/* Bad signature */
		debug(DBG_SOFT, "Invalid signature of the 0x%x check "
				"counter request", rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_INVALID_SIGNATURE);
		ERROR_FINISH(ret, -1, finish);
	}

	/* 
	 * Check if we are the real counter node or if we have to continue to 
	 * forward the pkt 
	 */
	andna_hash(my_family, req->pubkey, ANDNA_PKEY_LEN, pubk_hash, hash_gnode);
	if((err=find_hash_gnode(hash_gnode, &to, 0)) < 0) {
		debug(DBG_SOFT, "We are not the real (rounded)hash_gnode. "
				"Rejecting the 0x%x check_counter request",
				rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		ERROR_FINISH(ret, -1, finish);
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
				ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			ERROR_FINISH(ret, -1, finish);
		}

		/* Continue to forward the received pkt */
		debug(DBG_SOFT, "The check_counter rq pkt will be forwarded "
				"to: %s", inet_to_str(to));
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Finally, let's register/update the hname */
	cc=counter_c_add(&rfrom, req->pubkey);
	cch=cc_hashes_add(cc, req->hash);
	if(!cch) {
		debug(DBG_SOFT, "Request %s (0x%x) rejected: %s", 
				rq_to_str(rpkt.hdr.id), rpkt.hdr.id, 
				rq_strerror(E_ANDNA_TOO_MANY_HNAME));
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_ANDNA_TOO_MANY_HNAME);
		ERROR_FINISH(ret, -1, finish);
	}

	if(cch->hname_updates > req->hname_updates) {
		debug(DBG_SOFT, "Request %s (0x%x) rejected: hname_updates", 
			" mismatch", rq_to_str(rpkt.hdr.id), rpkt.hdr.id);
		if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK)
			ret=pkt_err(pkt, E_INVALID_REQUEST);
		ERROR_FINISH(ret, -1, finish);
	} else
		cch->hname_updates=req->hname_updates+1;
		
	/* Report the successful result to rfrom */
	if(rpkt.hdr.flags & ASYNC_REPLY && rpkt.hdr.flags & SEND_ACK) {
		debug(DBG_SOFT, "Check_counter rq 0x%x accepted.",
				rpkt.hdr.id);
		pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id, ACK_AFFERMATIVE, 0);
		pkt.msg=0;
		ret=forward_pkt(pkt, rfrom);
	}
	
	/* 
	 * Broadcast the request to the entire gnode of level 1 to let the
	 * other counter_nodes register the hname.
	 */

	pkt_copy(&flood_pkt, &rpkt);
	flood_pkt.sk=0;
	/* be sure that the other nodes don't reply to rfrom again */
	flood_pkt.hdr.flags&=~SEND_ACK & ~ASYNC_REPLY;
	
	/* If the pkt was sent from an our rnode, ignore it while flooding */
	real_from_rpos=ip_to_rfrom(flood_pkt.from, 0, 0, 0);
	if(real_from_rpos < 0) {
		exclude_function=exclude_glevel;
		real_from_rpos=-1;
	} else
		exclude_function=exclude_from_and_glevel;
	flood_pkt_send(exclude_function, 1, -1, real_from_rpos, flood_pkt);

finish: 
	if(ntop)
		xfree(ntop);
	if(rfrom_ntop)
		xfree(rfrom_ntop);
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


#ifndef ANDNA_DEBUG
	/*
	 * Search in the hostname in the local cache first. Maybe we are so
	 * dumb that we are trying to resolve the same ip we registered.
	 */
	if((lcl=lcl_cache_find_hname(andna_lcl, hname))) {
		memcpy(resolved_ip, &me.cur_ip, sizeof(inet_prefix));
		return ret;
	}
#endif
	
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
	if((err=find_hash_gnode(hash_gnode, &to, 1)) < 0)
		ERROR_FINISH(ret, -1, finish);
	else if(err == 1)
		req.flags|=ANDNA_FORWARD;
		
	ntop=inet_to_str(to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_RESOLVE_HNAME), ntop);
	
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
		ERROR_FINISH(ret, -1, finish);
	}

	if(rpkt.hdr.sz != ANDNA_RESOLVE_REPLY_PKT_SZ)
		ERROR_FINISH(ret, -1, finish);
		
	reply=(struct andna_resolve_reply_pkt *)rpkt.msg;
	inet_setip(resolved_ip, reply->ip, my_family);
	inet_htonl(resolved_ip);
	
	/* 
	 * Add the hostname in the resolved_hnames cache since it was
	 * successful resolved it ;)
	 */
	rh_cache_add(hname, reply->timestamp, resolved_ip);
	
finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 0);
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
	char *ntop=0, *rfrom_ntop=0;

	req=(struct andna_resolve_rq_pkt *)rpkt.msg;
	if(rpkt.hdr.sz != ANDNA_RESOLVE_RQ_PKT_SZ)
		ERROR_FINISH(ret, -1, finish);
	
	/* Save the real sender of the request */
	inet_setip(&rfrom, req->rip, my_family);
	inet_htonl(&rfrom);

	ntop=xstrdup(inet_to_str(rpkt.from));
	rfrom_ntop=xstrdup(inet_to_str(rfrom));
	debug(DBG_NOISE, "Andna Resolve request 0x%x from: %s, real from: %s",
			rpkt.hdr.id, ntop, rfrom_ntop);
	
	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	pkt_addsk(&pkt, my_family, 0, SKT_UDP);

	/*
	 * Are we the right hash_gnode or have we to still forward the pkt ?
	 */
	andna_hash_by_family(my_family, (u_char *)req->hash, hash_gnode);
	if((err=find_hash_gnode(hash_gnode, &to, 0)) < 0) {
		debug(DBG_SOFT, "We are not the real (rounded)hash_gnode. "
				"Rejecting the 0x%x resolve_hname request",
				rpkt.hdr.id);
		ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		goto finish;
	} else if(err == 1) {
		if(!(req->flags & ANDNA_FORWARD)) {
			ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
			goto finish;
		}

		/* Continue to forward the received pkt */
		debug(DBG_SOFT, "The resolve_hame rq pkt will be forwarded "
				"to: %s", inet_to_str(to));
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	/* Search the hostname to resolve in the andna_cache */
	andna_cache_del_expired();
	if(!(ac=andna_cache_findhash(req->hash))) {
		/* We don't have that hname in our andna_cache */
	
#if 0
		/*
		 * If our uptime is less than (ANDNA_EXPIRATION_TIME/2), then 
		 * we are a new hash_gnode that maybe has replaced another one,
		 * but since no more than (ANDNA_EXPIRATION_TIME/2) seconds 
		 * have passed we don't have the andna_cache for the hostname 
		 * the `rfrom' is asking us to resolve. What we have to do is
		 * asking to give us the andna_cache for that hname to the old 
		 * hash_gnode, we store it in our andna_c and we reply to 
		 * `rfrom'.
		 * Finally we broadcast the obtained andna_cache inside our
		 * gnode.
		 */
		if(me.uptime < (ANDNA_EXPIRATION_TIME/2)) {
			if(!get_andna_single_cache();)
		}
#endif
		/* There isn't it, bye. */
		debug(DBG_SOFT, "Request %s (0x%x) rejected: %s", 
				rq_to_str(rpkt.hdr.id), rpkt.hdr.id, 
				rq_strerror(E_ANDNA_NO_HNAME));
		ret=pkt_err(pkt, E_ANDNA_NO_HNAME);
		goto finish;
	}
	
	/* Send back the ip associated to the hname */
	debug(DBG_SOFT, "Resolve request 0x%x accepted", rpkt.hdr.id);
	memset(&reply, 0, sizeof(reply));
	memcpy(reply.ip, ac->acq->rip.data, MAX_IP_SZ);
	reply.timestamp=ac->acq->timestamp;
	pkt_fill_hdr(&pkt.hdr, ASYNC_REPLIED, rpkt.hdr.id, ANDNA_RESOLVE_REPLY,
			sizeof(reply));
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, &reply, sizeof(reply));
	ret=forward_pkt(pkt, rfrom);
	pkt_free(&pkt, 0);
	
finish:
	if(ntop)
		xfree(ntop);
	if(rfrom_ntop)
		xfree(rfrom_ntop);

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
		ERROR_FINISH(ret, -1, finish);
	}

	reply=(struct andna_rev_resolve_reply_hdr *)rpkt.msg;
	tot_hnames=reply->hostnames;
	tot_hnames++;
	if(tot_hnames > ANDNA_MAX_HOSTNAMES || tot_hnames <= 0)
		ERROR_FINISH(ret, -1, finish);

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
		hnames[i][hnames_sz[i]-1]=0;

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
	const char *ntop;
	int i, ret=0, err, hostnames;
	
	lcl_cache *alcl=andna_lcl;

	memset(&pkt, 0, sizeof(PACKET));

	ntop=inet_to_str(rpkt.from);
	debug(DBG_INSANE, "Andna reverse resolve request received 0x%x from %s",
			rpkt.hdr.id, ntop);
	
	/*
	 * Build the reply pkt
	 */
	
	pkt_fill_hdr(&pkt.hdr, 0, rpkt.hdr.id, ANDNA_RESOLVE_REPLY, 0);
	pkt.hdr.sz=sizeof(struct andna_rev_resolve_reply_hdr)+sizeof(u_short);
	
	hostnames=lcl_counter;
	hdr.hostnames=hostnames-1;
	if(hostnames) {
		hnames_sz=xmalloc(sizeof(u_short) * hostnames);
		i=0;
		list_for(alcl) {
			hnames_sz[i++]=strlen(alcl->hostname)+1;
			pkt.hdr.sz+=hnames_sz[i];
		}
	} else {
		ret=pkt_err(rpkt, E_ANDNA_NO_HNAME);
		goto finish;
	}

	debug(DBG_INSANE, "Reverse resolve request 0x%x accepted", rpkt.hdr.id);
	
	/* 
	 * Pack all the hostnames we have (if any) 
	 */

	pkt.msg=buf=xmalloc(pkt.hdr.sz);
	memcpy(buf, &hdr, sizeof(struct andna_rev_resolve_reply_hdr));
	
	if(hostnames) {
		buf+=sizeof(struct andna_rev_resolve_reply_hdr);

		memcpy(buf, hnames_sz, sizeof(u_short) * hostnames);
		buf+=sizeof(u_short) * hostnames;
		
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
        if(err==-1)
		ERROR_FINISH(ret, -1, finish);
finish:
	pkt_free(&pkt, 0);
	return ret;
}



/*****************************************************************************************************************************/
#ifdef EXPERIMENTAL_ANDNA_CODE
/*
 * get_single_andna_c: sends the ANDNA_GET_SINGLE_ACACHE request to the old 
 * `hash_gnode' of `hash' to retrieve the andna_cache that contains the
 * information about `hash'.
 */
andna_cache *get_single_andna_c(u_int hash[MAX_IP_INT],
		u_int hash_gnode[MAX_IP_INT])
{
	PACKET pkt, rpkt;
	struct single_acache_hdr req_hdr;
	andna_cache *andna_cache, *ret=0;
	inet_prefix *new_hgnodes;
	size_t pack_sz;
	int err, counter;
	const char *ntop;
	char *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	memset(&req_hdr, '\0', sizeof(PACKET));
	
	/* Find the old hash_gnode that corresponds to the hash `hash_gnode'*/
	new_hgnodes[0]=&me.cur_ip;
	if((err=find_hash_gnode(hash_gnode, &pkt.to, new_hgnodes, 1, 1)) < 0) {
		debug(DBG_SOFT, "get_single_andna_c: old hash_gnode not found");
		ERROR_FINISH(ret, -1, finish);
	} else if(err == 1)
		req_hdr.flags|=ANDNA_FORWARD;
	
	req_hdr.hgnodes=1;
	memcpy(req_hdr.rip, me.cur_ip.data, MAX_IP_SZ);
	memcpy(req_hdr.hash, hash, MAX_IP_SZ);

	pkt.hdr.flags|=ASYNC_REPLY;
	pkt.hdr.sz=SINGLE_ACACHE_PKT_SZ(1);
	pkt.msg=xmalloc(pkt.hdr.sz);
	memcpy(pkt.msg, &req_hdr, pkt.hdr.sz);
	memcpy(pkt.msg+sizeof(struct single_acache_hdr), me.cur_ip.data,
			MAX_IP_SZ);
	
	ntop=inet_to_str(pkt.to);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(ANDNA_GET_SINGLE_ACACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_GET_SINGLE_ACACHE, 0, ACK_AFFERMATIVE, 1, &rpkt);
	if(err==-1)
		ERROR_FINISH(ret, -1, finish);
	
	pack_sz=rpkt.hdr.sz;
	pack=rpkt.msg;
	ret=andna_cache=unpack_andna_cache(pack, pack_sz, &counter);
	if(!andna_cache) {
		error("get_single_acache(): Malformed andna_cache.");
		ERROR_FINISH(ret, -1, finish);
	}
	
finish:
	pkt_free(&pkt, 1);
	pkt_free(&rpkt, 0);
	return ret;
}

/*
 * TODO: COMMENT HERE
 */
int put_single_acache(PACKET rpkt)
{
	PACKET pkt;
	struct single_acache_hdr *req_hdr;
	u_int hash_gnode[MAX_IP_INT];
	inet_prefix rfrom, to, *new_hgnodes=0;
	andna_cache *ac, *ac_tmp;
	char *buf;
	char *ntop=0, *rfrom_ntop=0;
	int ret, i;
	ssize_t err;
	size_t pkt_sz=0;
	
	req_hdr=rpkt.msg;
	if(rpkt.hdr.sz != SINGLE_ACACHE_PKT_SZ(req_hdr->hgnodes) ||
			req_hdr->hgnodes > ANDNA_MAX_NEW_GNODES)
		ERROR_FINISH(ret, -1, finish);

	/* Save the real sender of the request */
	inet_setip(&rfrom, req_hdr->rip, my_family);
	inet_htonl(&rfrom);

	ntop=xstrdup(inet_to_str(rpkt.from));
	rfrom_ntop=xstrdup(inet_to_str(rfrom));
	debug(DBG_NOISE, "Andna get single cache request 0x%x from: %s, real "
			"from: %s", rpkt.hdr.id, ntop, rfrom_ntop);
	
	memcpy(&pkt, &rpkt, sizeof(PACKET));
	memcpy(&pkt.from, &rfrom, sizeof(inet_prefix));
	pkt_addsk(&pkt, my_family, 0, SKT_UDP);

	new_hgnodes=xmalloc(sizeof(inet_prefix *) * (req_hdr->hgnodes+1));
	buf=rpkt.msg+sizeof(struct single_acache_hdr);
	for(i=0; i<req_hdr->hgnodes; i++) {
		new_hgnodes[i]=buf;
		buf+=MAX_IP_SZ;
	}

	andna_hash_by_family(my_family, (u_char *)req->hash, hash_gnode);
	if((err=find_hash_gnode(hash_gnode, &to, new_hgnodes, 
					req_hdr->hgnodes, 0)) < 0) {
		debug(DBG_SOFT, "We are not the real (rounded)hash_gnode. "
				"Rejecting the 0x%x get_single_acache request",
				rpkt.hdr.id);
		ret=pkt_err(pkt, E_ANDNA_WRONG_HASH_GNODE);
		goto finish;
	} else if(err == 1) {
		/* Continue to forward the received pkt */
		debug(DBG_SOFT, "The 0x%x rq pkt will be forwarded "
				"to: %s", rpkt.hdr.id, inet_to_str(to));
		ret=forward_pkt(rpkt, to);
		goto finish;
	}

	if(me.uptime < (ANDNA_EXPIRATION_TIME/2)) {
		new_hgnodes[req_hdr->hgnodes]=&me.cur_ip;
		if((err=find_old_hash_gnode(hash_gnode, &to, new_hgnodes, 
						req_hdr->hgnodes+1, 1)) < 0) {
			debug(DBG_SOFT, "put_single_andna_c: old hash_gnode not found");
			ERROR_FINISH(ret, -1, finish);
		}

		memcpy(&pkt, &rpkt, sizeof(PACKET));
		pkt.sk=0;
		pkt.hdr.sz=rpkt.hdr.sz+MAX_IP_SZ;
		pkt.msg=xmalloc(pkt.hdr.sz);
		memcpy(pkt.msg, rpkt.msg, rpkt.hdr.sz);
		memcpy(pkt.msg+rpkt.hdr.sz, me.cur_ip.data, MAX_IP_SZ);

		ret=forward_pkt(pkt, to);
		pkt_free(&pkt, 1);
		goto finish;
	}
		
	andna_cache_del_expired();
	if(!(ac=andna_cache_findhash(req_hdr->hash))) {
		/* There isn't it, bye. */
		debug(DBG_SOFT, "Request %s (0x%x) rejected: %s", 
				rq_to_str(rpkt.hdr.id), rpkt.hdr.id, 
				rq_strerror(E_ANDNA_NO_HNAME));
		ret=pkt_err(pkt, E_ANDNA_NO_HNAME);
		goto finish;
	}
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &req_hdr.rip);
	pkt_addsk(&pkt, my_family, 0, SKT_TCP);

	ac_tmp=xmalloc(sizeof(andna_cache));
	memset(ac_tmp, 0, sizeof(andna_cache));
	list_copy(ac_tmp, ac);
	pkt.msg=pack_andna_cache(ac_tmp, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply put_single_acache to %s", ntop);
	err=send_rq(&pkt, ASYNC_REPLIED, ACK_AFFERMATIVE, rpkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_andna_cache(): Cannot send the put_single_acache "
				"reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}
finish:
	if(ac_tmp)
		xfree(ac_tmp);
	if(new_hgnodes)
		xfree(new_hgnodes);
	if(ntop)
		xfree(ntop);
	if(rfrom_ntop)
		xfree(rfrom_ntop);

	pkt_free(&pkt, 0);
	return ret;
}

#endif
/*****************************************************************************************************************************/



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
		error("get_andna_cache(): Malformed or empty andna_cache. "
				"Cannot load it");
	
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
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_andna_cache(andna_c, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(ANDNA_PUT_ANDNA_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_PUT_ANDNA_CACHE, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_andna_cache(): Cannot send the ANDNA_PUT_ANDNA_CACHE reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
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
		error("get_counter_cache(): Malformed or empty counter_cache. Cannot load it");
	
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
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_counter_cache(andna_counter_c, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(ANDNA_PUT_COUNT_CACHE), ntop);
	err=send_rq(&pkt, 0, ANDNA_PUT_COUNT_CACHE, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_counter_cache(): Cannot send the ANDNA_PUT_COUNT_CACHE reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
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

	if(!me.cur_node->links)
		/* nothing to do */
		return 0;

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
	int ret, updates=0;

	list_for(alcl) {
		if(alcl->timestamp)
			continue;
		ret=andna_register_hname(alcl);
		if(!ret) {
			loginfo("Hostname \"%s\" registered/updated "
					"successfully", alcl->hostname);
			updates++;
		}
	}
	if(updates)
		save_lcl_cache(&lcl_keyring, andna_lcl, server_opt.lcl_file);
}

/*
 * andna_maintain_hnames_active: periodically registers and keep up to date the
 * hostnames of the local cache.
 */
void *andna_maintain_hnames_active(void *null)
{
	lcl_cache *alcl;
	int ret, updates;

	for(;;) {
		updates=0;
		alcl=andna_lcl;
		
		list_for(alcl) {
			ret=andna_register_hname(alcl);
			if(!ret) {
				loginfo("Hostname \"%s\" registered/updated "
						"successfully", alcl->hostname);
				updates++;
			}
		}
		
		if(updates)
			save_lcl_cache(&lcl_keyring, andna_lcl, server_opt.lcl_file);

#ifdef ANDNA_DEBUG
		sleep(4);
//		for(;;) {
			if(andna_lcl && !andna_c) {
				inet_prefix ip;
				debug(DBG_INSANE, "Trying to resolve \"netsukuku\"");
				if(!andna_resolve_hname("netsukuku", &ip))
					debug(DBG_INSANE, "Resolved! ip: %s", inet_to_str(ip));
				else
					debug(DBG_INSANE, "Resolved failure Something went wrong");
			}
#if 0	
			if(!andna_lcl && andna_c) {
				int h, i;
				char **hh;
					debug(DBG_INSANE, "Trying to rev resolve %s", inet_to_str(andna_c->acq->rip));
				h=andna_reverse_resolve(andna_c->acq->rip, &hh);
				if(h > 0) {
					for(i=0; i<h; i++)
						debug(DBG_INSANE, "%s -> %s", inet_to_str(andna_c->acq->rip), hh[i]);
				} else
					debug(DBG_INSANE, "Rev resolve failed");
			}
#endif
			
//			sleep(ANDNA_EXPIRATION_TIME + rand_range(1, 10));
//		}
#endif
		sleep((ANDNA_EXPIRATION_TIME/2) + rand_range(1, 10));
	}

	return 0;
}

void *andna_main(void *null)
{
	struct udp_daemon_argv ud_argv;
	u_short *port;
	pthread_t thread;
	pthread_attr_t t_attr;
	
	pthread_attr_init(&t_attr);
	pthread_attr_setdetachstate(&t_attr, PTHREAD_CREATE_DETACHED);

	memset(&ud_argv, 0, sizeof(struct udp_daemon_argv));
	port=xmalloc(sizeof(u_short));

	pthread_mutex_init(&udp_daemon_lock, 0);
	pthread_mutex_init(&tcp_daemon_lock, 0);

	debug(DBG_SOFT,   "Evocating the andna udp daemon.");
	ud_argv.port=andna_udp_port;
	ud_argv.flags|=UDP_THREAD_FOR_EACH_PKT;
	pthread_mutex_lock(&udp_daemon_lock);
	pthread_create(&thread, &t_attr, udp_daemon, (void *)&ud_argv);
	pthread_mutex_lock(&udp_daemon_lock);
	pthread_mutex_unlock(&udp_daemon_lock);

	debug(DBG_SOFT,   "Evocating the andna tcp daemon.");
	*port=andna_tcp_port;
	pthread_mutex_lock(&tcp_daemon_lock);
	pthread_create(&thread, &t_attr, tcp_daemon, (void *)port);
	pthread_mutex_lock(&tcp_daemon_lock);
	pthread_mutex_unlock(&tcp_daemon_lock);

#ifndef DEBUG /* XXX */
#warning The ANDNA hook is disabled for debugging purpose
	/* Start the ANDNA hook */
	pthread_create(&thread, &t_attr, andna_hook, 0);
#endif

	/* Start the hostnames updater and register */
	andna_maintain_hnames_active(0);
	
	xfree(port);
	return 0;
}
