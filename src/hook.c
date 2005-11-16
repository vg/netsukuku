/* This file is part of Netsukuku
 * (c) Copyright 2005 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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
 *
 * --
 *  
 * hook.c:
 * This is code which handles the hooking of a new node in netsukuku, or the
 * creation of a new gnode.
 */

#include "includes.h"

#include "misc.h"
#include "libnetlink.h"
#include "ll_map.h"
#include "inet.h"
#include "if.h"
#include "krnl_route.h"
#include "endianness.h"
#include "bmap.h"
#include "route.h"
#include "request.h"
#include "pkts.h"
#include "tracer.h"
#include "qspn.h"
#include "hook.h"
#include "radar.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"

int free_the_tmp_cur_node;

/*
 * verify_free_nodes_hdr: verifies the validity of the `fn_hdr'
 * free_nodes_hdr. `to' is the ip of the node which sent the 
 * put_free_nodes reply.
 * If the header is valid 0 is returned.
 */
int verify_free_nodes_hdr(inet_prefix *to, struct free_nodes_hdr *fn_hdr)
{
	inet_prefix ipstart;
	int ip_match_sz;

	/* If fn_hdr->ipstart != `to' there is an error */
	inet_setip(&ipstart, fn_hdr->ipstart, my_family);
	ip_match_sz=sizeof(u_char)*(GET_LEVELS(my_family)-1);
	if(memcmp(ipstart.data, to->data, ip_match_sz))
		return 1;

	if(fn_hdr->nodes <= 0 || fn_hdr->nodes == (MAXGROUPNODE-1))
		return 1;
	
	if(fn_hdr->max_levels > GET_LEVELS(my_family) || !fn_hdr->level)
		return 1;
	
	if(fn_hdr->level >= fn_hdr->max_levels)
		return 1;

	return 0;
}

/*  
 *  *  *  put/get free_nodes  *  *  *
 */

/* 
 * get_free_nodes: It send the GET_FREE_NODES request, used to retrieve the free_nodes
 * pkt (see hook.h).
 * `fn_hdr' is the header of the received free_nodes packet.
 * `nodes' must be an u_char array with at least MAXGROUPNODES members. All the
 * members that go from `free_nodes[0]' to `free_nodes[fn_hdr.nodes]' will be
 * filled with the gids of the received free nodes.
 */
int get_free_nodes(inet_prefix to, interface *dev, 
		struct free_nodes_hdr *fn_hdr, u_char *nodes)
{
	PACKET pkt, rpkt;
	ssize_t err;
	int ret=0, e, i;
	const char *ntop;
	char *buf=0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	pkt_add_dev(&pkt, dev, 1);
	
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(GET_FREE_NODES), ntop);
	err=send_rq(&pkt, 0, GET_FREE_NODES, 0, PUT_FREE_NODES, 1, &rpkt);
	if(err==-1)
		ERROR_FINISH(ret, -1, finish);
	
	memcpy(fn_hdr, rpkt.msg, sizeof(struct free_nodes_hdr));
	if(verify_free_nodes_hdr(&to, fn_hdr)) {
		error("Malformed PUT_FREE_NODES request hdr from %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}
	
	fn_hdr->nodes++;
	
	buf=rpkt.msg+sizeof(struct free_nodes_hdr);

	for(i=0, e=0; i<MAXGROUPNODE; i++) {
		if(TEST_BIT(buf, i)) {
			nodes[e]=i;
			e++;
		}
	}

	debug(DBG_NORMAL, "Received %d free %s", fn_hdr->nodes, 
			fn_hdr->level == 1 ? "nodes" : "gnodes");
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt,1);
	return ret;
}

/* 
 * put_free_nodes: It sends a free_nodes pkt to rq_pkt.from. To see what's a
 * free_nodes pkt go in hook.h.
 */
int put_free_nodes(PACKET rq_pkt)
{	
	struct fn_pkt {
		struct free_nodes_hdr fn_hdr;
		u_char free_nodes[MAXGROUPNODE/8];
	}_PACKED_ fn_pkt;

	PACKET pkt;
	int ret=0, i, e=0;
	ssize_t err, pkt_sz;
	u_char level;
	const char *ntop;
	char *p=0; 
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_FREE_NODES reply to %s", ntop);
	
	memset(&pkt, 0, sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, 0);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);
	pkt_add_dev(&pkt, rq_pkt.dev, 1);

	/* We search in each level a gnode which is not full. */
	for(level=1, e=0; level < me.cur_quadg.levels; level++) {
		if(!(me.cur_quadg.gnode[_EL(level)]->flags & GMAP_FULL)) {
			e=1;
			break;
		}
	}
	if(!e) {
		/* <<My quadro_group is completely full, sry>> */
		pkt_fill_hdr(&pkt.hdr, HOOK_PKT, rq_pkt.hdr.id, PUT_FREE_NODES, 0);
		err=pkt_err(pkt, E_QGROUP_FULL);
		goto finish;
	}

	/* Ok, we've found one, so let's roll the pkt */
	memset(&fn_pkt, 0, sizeof(fn_pkt));

	fn_pkt.fn_hdr.max_levels=me.cur_quadg.levels;
	inet_copy_ipdata(fn_pkt.fn_hdr.ipstart, &me.cur_quadg.ipstart[level]);
	fn_pkt.fn_hdr.level=level;
	fn_pkt.fn_hdr.gid=me.cur_quadg.gid[level];
	
	/*
	 * Creates the list of the free nodes, which belongs to the gnode. If 
	 * the gnode level is 1 it scans the int_map to find all the MAP_VOID 
	 * nodes, otherwise it scans the gnode map at level-1 searching for 
	 * GMAP_VOID gnodes.
	 */
	e=0;
	if(level == 1) {
		for(i=0; i<MAXGROUPNODE; i++)
			if(me.int_map[i].flags & MAP_VOID) {
				SET_BIT(fn_pkt.free_nodes, i);
				e++;
			}
	} else {
		for(i=0; i<MAXGROUPNODE; i++)
			if(me.ext_map[_EL(level-1)][i].flags & GMAP_VOID ||
					me.ext_map[_EL(level-1)][i].g.flags & MAP_VOID) {
				SET_BIT(fn_pkt.free_nodes, i);
				e++;
			}
	}
	fn_pkt.fn_hdr.nodes=(u_char)e-1;
	
	/* Go pkt, go! Follow your instinct */
	pkt_sz=FREE_NODES_SZ((fn_pkt.fn_hdr.nodes+1));
	pkt_fill_hdr(&pkt.hdr, HOOK_PKT, rq_pkt.hdr.id, PUT_FREE_NODES, pkt_sz);
	pkt.msg=xmalloc(pkt_sz);
	memset(pkt.msg, 0, pkt_sz);
	
	p=pkt.msg;
	memcpy(p, &fn_pkt, sizeof(fn_pkt));
	
	err=pkt_send(&pkt);
	
finish:	
	if(err==-1) {
		error("put_free_nodes(): Cannot send the PUT_FREE_NODES reply to %s.", ntop);
		ret=-1;
	}
	pkt_free(&pkt, 0);
	return ret;
}


/*
 *  *  * put/get qspn_round *  *  *
 */

/* 
 * get_qspn_round: It send the GET_QSPN_ROUND request, used to retrieve the 
 * qspn ids and and qspn times. (see hook.h).
 */
int get_qspn_round(inet_prefix to, interface *dev, struct timeval to_rtt, 
		struct timeval *qtime, int *qspn_id, int *qspn_gcount)
{
	PACKET pkt, rpkt;
	struct timeval cur_t;
	ssize_t err;
	int ret=0, level;
	const char *ntop;
	char *buf=0;
	u_char max_levels;
	u_int *gcount;

	int_info qr_pkt_iinfo;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(GET_QSPN_ROUND), ntop);
	pkt_add_dev(&pkt, dev, 1);
	err=send_rq(&pkt, 0, GET_QSPN_ROUND, 0, PUT_QSPN_ROUND, 1, &rpkt);
	if(err==-1)
		ERROR_FINISH(ret, -1, finish);
	
	memcpy(&max_levels, rpkt.msg, sizeof(u_char));
	if(QSPN_ROUND_PKT_SZ(max_levels) != rpkt.hdr.sz ||
			max_levels > GET_LEVELS(my_family)) {
		error("Malformed PUT_QSPN_ROUND request hdr from %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}
	
	/* Convert the pkt from network to host order */
	int_info_copy(&qr_pkt_iinfo, &qspn_round_pkt_iinfo);
	qr_pkt_iinfo.int_offset[1] = me.cur_quadg.levels*sizeof(int)+sizeof(char);
	qr_pkt_iinfo.int_offset[2] = qr_pkt_iinfo.int_offset[1] + sizeof(struct timeval)*max_levels;
	qr_pkt_iinfo.int_nmemb[0]  = max_levels;
	qr_pkt_iinfo.int_nmemb[1]  = max_levels*2;
	ints_network_to_host(rpkt.msg, qr_pkt_iinfo);

	/* Restoring the qspn_id and the qspn_round time */
	buf=rpkt.msg+sizeof(u_char);
	memcpy(qspn_id, buf, max_levels * sizeof(int));
	
	buf+=max_levels * sizeof(int);
	memcpy(qtime, buf, max_levels * sizeof(struct timeval));
	
	gettimeofday(&cur_t, 0);
	for(level=0; level < max_levels; level++) {
		timeradd(&to_rtt, &qtime[level], &qtime[level]);
#ifdef DEBUG
		debug(DBG_INSANE, "qspn_id: %d qtime[%d] set to %d, to_rtt: %d", 
				qspn_id[level], level, 
				MILLISEC(qtime[level]), MILLISEC(to_rtt));
#endif
		timersub(&cur_t, &qtime[level], &qtime[level]);
	}

	/* Extracting the qspn_gnode_count */
	buf+=max_levels * sizeof(struct timeval);
	gcount=(u_int *)buf;
	for(level=0; level < GCOUNT_LEVELS; level++)
		qspn_inc_gcount(qspn_gcount, level, gcount[level]);

finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt,1);
	return ret;
}
/* 
 * put_qspn_round: It sends the current qspn times and ids to rq_pkt.from. 
 */
int put_qspn_round(PACKET rq_pkt)
{	
	struct qspn_round_pkt {
		u_char		max_levels;
		int32_t		qspn_id[me.cur_quadg.levels];
		struct timeval  qtime[me.cur_quadg.levels];
		u_int		gcount[GCOUNT_LEVELS];
	}_PACKED_ qr_pkt;
	int_info qr_pkt_iinfo;

	PACKET pkt;
	struct timeval cur_t;
	int ret=0;
	ssize_t err, pkt_sz;
	u_char level;
	const char *ntop;

	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_QSPN_ROUND reply to %s", ntop);
	
	memset(&pkt, 0, sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_udp_port);
	pkt_addflags(&pkt, 0);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);
	pkt_add_dev(&pkt, rq_pkt.dev, 1);

	/* We fill the qspn_id and the qspn round time */
	qr_pkt.max_levels=me.cur_quadg.levels;
	memcpy(qr_pkt.qspn_id, me.cur_qspn_id, sizeof(int) * qr_pkt.max_levels);
	
	gettimeofday(&cur_t, 0);
	for(level=0; level < qr_pkt.max_levels; level++) {
		update_qspn_time(level, 0);
		timersub(&cur_t, &me.cur_qspn_time[level], &qr_pkt.qtime[level]);
		debug(DBG_INSANE, "qspn_id: %d, qr_pkt.qtime[%d]: %d", 
				qr_pkt.qspn_id[level], level,
				MILLISEC(qr_pkt.qtime[level]));
	}

	/* copy in the pkt the qspn_gnode_count */
	memcpy(qr_pkt.gcount, qspn_gnode_count, sizeof(qspn_gnode_count));

	/* fill the PKT header */
	pkt_sz=sizeof(qr_pkt);
	pkt_fill_hdr(&pkt.hdr, HOOK_PKT, rq_pkt.hdr.id, PUT_QSPN_ROUND, pkt_sz);
	pkt.msg=xmalloc(pkt_sz);
	memset(pkt.msg, 0, pkt_sz);
	
	/* Convert the pkt from host to network order */
	int_info_copy(&qr_pkt_iinfo, &qspn_round_pkt_iinfo);
	qr_pkt_iinfo.int_offset[1] = me.cur_quadg.levels*sizeof(int)+sizeof(char);
	qr_pkt_iinfo.int_offset[2] = qr_pkt_iinfo.int_offset[1] + 
						sizeof(struct timeval)*qr_pkt.max_levels;
	qr_pkt_iinfo.int_nmemb[0]  = me.cur_quadg.levels;
	qr_pkt_iinfo.int_nmemb[1]  = me.cur_quadg.levels*2;
	ints_host_to_network(&qr_pkt, qr_pkt_iinfo);
	
	/* Go pkt, go! Follow your instinct */
	memcpy(pkt.msg, &qr_pkt, sizeof(qr_pkt));
	err=pkt_send(&pkt);
	
	if(err==-1) {
		error("put_qspn_round(): Cannot send the PUT_QSPN_ROUND reply to %s.", ntop);
		ret=-1;
	}
	pkt_free(&pkt, 0);
	return ret;
}


/*  
 *  *  *  put/get ext_map  *  *  *
 */

int put_ext_map(PACKET rq_pkt)
{
	PACKET pkt;
	const char *ntop; 
	int ret=0;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_EXT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);
	pkt_add_dev(&pkt, rq_pkt.dev, 1);

	pkt.msg=pack_extmap(me.ext_map, MAXGROUPNODE, &me.cur_quadg, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(PUT_EXT_MAP), ntop);
	err=send_rq(&pkt, 0, PUT_EXT_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_ext_maps(): Cannot send the PUT_EXT_MAP reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}

finish:
	pkt_free(&pkt, 1);
	return ret;
}

/* 
 * get_ext_map: It sends the GET_EXT_MAP request to retrieve the
 * dst_node's ext_map.
 */
map_gnode **get_ext_map(inet_prefix to, interface *dev, quadro_group *new_quadg)
{
	PACKET pkt, rpkt;
	const char *ntop;
	char *pack;
	int err;
	map_gnode **ext_map=0, **ret=0;

	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	pkt_add_dev(&pkt, dev, 1);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(GET_EXT_MAP), ntop);
	
	err=send_rq(&pkt, 0, GET_EXT_MAP, 0, PUT_EXT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	pack=rpkt.msg;
	ret=ext_map=unpack_extmap(pack, new_quadg);
	if(!ext_map)
		error("get_ext_map: Malformed ext_map. Cannot unpack the ext_map.");
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
	return ret;
}

/*  
 *  *  *  put/get int_map  *  *  *
 */

int put_int_map(PACKET rq_pkt)
{
	PACKET pkt;
	map_node *map=me.int_map;
	const char *ntop; 
	int ret=0;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_INT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);
	pkt_add_dev(&pkt, rq_pkt.dev, 1);

	pkt.msg=pack_map(map, 0, MAXGROUPNODE, me.cur_node, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	debug(DBG_INSANE, "Reply %s to %s", re_to_str(PUT_INT_MAP), ntop);
	err=send_rq(&pkt, 0, PUT_INT_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_int_map(): Cannot send the PUT_INT_MAP reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}
finish:
	pkt_free(&pkt, 0);
	return ret;
}

/* 
 * get_int_map: It sends the GET_INT_MAP request to retrieve the 
 * dst_node's int_map. 
 */
map_node *get_int_map(inet_prefix to, interface *dev, map_node **new_root)
{
	PACKET pkt, rpkt;
	map_node *int_map, *ret=0;
	int err;
	const char *ntop;
	char *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	pkt_add_dev(&pkt, dev, 1);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(GET_INT_MAP), ntop);
	err=send_rq(&pkt, 0, GET_INT_MAP, 0, PUT_INT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	pack=rpkt.msg;
	ret=int_map=unpack_map(pack, 0, new_root, MAXGROUPNODE, 
			MAXRNODEBLOCK_PACK_SZ);
	if(!int_map)
		error("get_int_map(): Malformed int_map. Cannot load it");
	
	/*Finished, yeah*/
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
	return ret;
}

/*  
 *  *  *  put/get bnode_map  *  *  *
 */

int put_bnode_map(PACKET rq_pkt)
{
	PACKET pkt;
	map_bnode **bmaps=me.bnode_map;
	const char *ntop; 
	int ret=0;
	ssize_t err;
	size_t pack_sz=0;
	
	ntop=inet_to_str(rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_BNODE_MAP reply to %s", ntop);

	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, my_family, rq_pkt.sk, rq_pkt.sk_type);
	pkt_add_dev(&pkt, rq_pkt.dev, 1);

	pkt.msg=pack_all_bmaps(bmaps, me.bmap_nodes, me.ext_map, me.cur_quadg, &pack_sz);
	pkt.hdr.sz=pack_sz;

	debug(DBG_INSANE, "Reply %s to %s", re_to_str(PUT_BNODE_MAP), ntop);
	err=send_rq(&pkt, 0, PUT_BNODE_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_bnode_maps(): Cannot send the PUT_BNODE_MAP reply to %s.", ntop);
		ERROR_FINISH(ret, -1, finish);
	}

finish:
	pkt_free(&pkt, 0);
	return ret;
}

/* 
 * get_bnode_map: It sends the GET_BNODE_MAP request to retrieve the 
 * dst_node's bnode_map. 
 */
map_bnode **get_bnode_map(inet_prefix to, interface *dev, u_int **bmap_nodes)
{
	PACKET pkt, rpkt;
	int err;
	map_bnode **bnode_map, **ret=0;
	const char *ntop;
	char *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(to);
	
	pkt_addto(&pkt, &to);
	pkt_add_dev(&pkt, dev, 1);
	debug(DBG_INSANE, "Quest %s to %s", rq_to_str(GET_BNODE_MAP), ntop);
	err=send_rq(&pkt, 0, GET_BNODE_MAP, 0, PUT_BNODE_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	/* Extracting the map... */
	pack=rpkt.msg;
	ret=bnode_map=unpack_all_bmaps(pack, GET_LEVELS(my_family), me.ext_map, bmap_nodes, 
			MAXGROUPNODE, MAXBNODE_RNODEBLOCK);
	if(!bnode_map)
		error("get_bnode_map(): Malformed bnode_map. Cannot load it");

finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
	return ret;
}


/* 
 * set_ip_and_def_gw: Set the same `ip' to all the devices.
 */
void hook_set_all_ips(inet_prefix ip, interface *ifs, int ifs_n)
{
	const char *ntop;
	ntop=inet_to_str(ip);
	
	loginfo("Setting the %s ip to all the interfaces", ntop);

	if(my_family == AF_INET) {
		/* Down & Up: reset the configurations of all the interfaces */
		set_all_ifs(ifs, ifs_n, set_dev_down);
		set_all_ifs(ifs, ifs_n, set_dev_up);
	} else {
		ip_addr_flush_all_ifs(ifs, ifs_n, my_family, RT_SCOPE_UNIVERSE);
		ip_addr_flush_all_ifs(ifs, ifs_n, my_family, RT_SCOPE_SITE);
	}

	if(set_all_dev_ip(ip, ifs, ifs_n) < 0)
		fatal("Cannot set the %s ip to all the interfaces", ntop);

#if 0
	/* From the 0.0.4b version there is no more need to replace the
	 * default gw because there is the support for the multi-interfaces.
	 */

	/*
	 * We set the default gw to our ip so to avoid the subnetting shit.
	 * Bleah, Class A, B, C, what a fuck -_^ 
	 */
	if(!server_opt.restricted) {
		debug(DBG_NORMAL, "Setting the default gw to %s.", ntop);
		if(rt_replace_def_gw(dev, ip))
			fatal("Cannot set the default gw to %s for the %s dev", 
					ntop, dev);
	}
#endif
}

/*
 * create_gnodes: This function is used to create a new gnode (or more) when
 * we are the first node in the area or when all the other gnodes are
 * full. 
 * Our ip will be set to `ip'. If `ip' is NULL, a random ip is chosen. 
 * create_gnodes() sets also all the vital variables for the new gnode/gnodes
 * like me.cur_quadg, me.cur_ip, etc...
 * `final_level' is the highest level where we create the gnode, all the other
 * gnodes we create are in the sub-levels of `final_level'. 
 */
int create_gnodes(inet_prefix *ip, int final_level)
{
	int i;

	if(!ip) {
		for(;;) {
			random_ip(0, 0, 0, GET_LEVELS(my_family), me.ext_map, 0, 
					&me.cur_ip, my_family);
			if(!inet_validate_ip(me.cur_ip))
				break;
		}
	} else
		memcpy(&me.cur_ip, ip, sizeof(inet_prefix));

	if(server_opt.restricted)
		inet_setip_localaddr(&me.cur_ip, my_family);

	if(!final_level)
		final_level=GET_LEVELS(my_family);
	
	/* 
	 * We remove all the traces of the old gnodes in the ext_map to add the
	 * new ones.
	 */
	if(!(me.cur_node->flags & MAP_HNODE))
		for(i=1; i<final_level; i++) {
			me.cur_quadg.gnode[_EL(i)]->flags &= ~GMAP_ME;
			me.cur_quadg.gnode[_EL(i)]->g.flags &= ~MAP_ME & ~MAP_GNODE;
		}

	/* Now, we update the ext_map with the new gnodes */
	reset_extmap(me.ext_map, final_level, 0);
	me.cur_quadg.levels=GET_LEVELS(my_family);
	iptoquadg(me.cur_ip, me.ext_map, &me.cur_quadg, QUADG_GID|QUADG_GNODE|QUADG_IPSTART);
	
	for(i=1; i<final_level; i++) {
		me.cur_quadg.gnode[_EL(i)]->flags &= ~GMAP_VOID;
		me.cur_quadg.gnode[_EL(i)]->flags |=  GMAP_ME;
		me.cur_quadg.gnode[_EL(i)]->g.flags&=~ MAP_VOID;
		me.cur_quadg.gnode[_EL(i)]->g.flags |= MAP_ME | MAP_GNODE;

		/* Increment the gnode seeds counter */
		gnode_inc_seeds(&me.cur_quadg, i);
	}
	
	/* Tidying up the internal map */
	if(free_the_tmp_cur_node) {
		free(me.cur_node);
		free_the_tmp_cur_node=0;
	}
	reset_int_map(me.int_map, 0);
	me.cur_node = &me.int_map[me.cur_quadg.gid[0]];
	me.cur_node->flags &= ~MAP_VOID;
	me.cur_node->flags |= MAP_ME;

	return 0;
}


int hook_init(void)
{
	/* register the hook's ops in the pkt_op_table */
	add_pkt_op(GET_FREE_NODES, SKT_TCP, ntk_tcp_port, put_free_nodes);
	add_pkt_op(PUT_FREE_NODES, SKT_TCP, ntk_tcp_port, 0);
	add_pkt_op(GET_QSPN_ROUND, SKT_TCP, ntk_tcp_port, put_qspn_round);
	add_pkt_op(PUT_QSPN_ROUND, SKT_TCP, ntk_tcp_port, 0);
	add_pkt_op(GET_INT_MAP, SKT_TCP, ntk_tcp_port, put_int_map);
	add_pkt_op(PUT_INT_MAP, SKT_TCP, ntk_tcp_port, 0);
	add_pkt_op(GET_EXT_MAP, SKT_TCP, ntk_tcp_port, put_ext_map);
	add_pkt_op(PUT_EXT_MAP, SKT_TCP, ntk_tcp_port, 0);
	add_pkt_op(GET_BNODE_MAP, SKT_TCP, ntk_tcp_port, put_bnode_map);
	add_pkt_op(PUT_BNODE_MAP, SKT_TCP, ntk_tcp_port, 0);
	
	if(my_family == AF_INET) {
		debug(DBG_NORMAL, "Deleting the loopback network (leaving only"
				" 127.0.0.1)");
		rt_del_loopback_net();
	}
	
	debug(DBG_NORMAL, "Activating ip_forward and disabling rp_filter");
	route_ip_forward(my_family, 1);
	route_rp_filter_all_dev(my_family, me.cur_ifs, me.cur_ifs_n, 0);

	total_hooks=0;

	return 0;
}

void hook_reset(void)
{
	u_int idata[MAX_IP_INT];

	/* We use a fake root_node for a while */
	free_the_tmp_cur_node=1;
	me.cur_node=xmalloc(sizeof(map_node));
	memset(me.cur_node, 0, sizeof(map_node));
	me.cur_node->flags|=MAP_HNODE;
	
	/*
	 * We set the dev ip to HOOKING_IP+random_number to begin our 
	 * transaction. 
	 */
	memset(idata, 0, MAX_IP_SZ);
	if(my_family==AF_INET) 
		idata[0]=HOOKING_IP;
	else
		idata[0]=HOOKING_IP6;
	
	idata[0]=ntohl(idata[0]);
	if(my_family == AF_INET6)
		idata[3]+=rand_range(0, MAXGROUPNODE-2);
	else
		idata[0]+=rand_range(0, MAXGROUPNODE-2);
	idata[0]=htonl(idata[0]);

	inet_setip(&me.cur_ip, idata, my_family);
	iptoquadg(me.cur_ip, me.ext_map, &me.cur_quadg,	
			QUADG_GID|QUADG_GNODE|QUADG_IPSTART);
	
	hook_set_all_ips(me.cur_ip, me.cur_ifs, me.cur_ifs_n);
}
		
/*
 * netsukuku_hook: hooks at an existing gnode or creates a new one.
 */
int netsukuku_hook(void)
{	
	struct radar_queue *rq=radar_q;
	struct free_nodes_hdr fn_hdr;
	
	map_node **merg_map, *new_root;
	map_gnode **old_ext_map;
	map_bnode **old_bnode_map;	
	
	inet_prefix gnode_ipstart;
	
	int i=0, e=0, imaps=0, ret=0, new_gnode=0, tracer_levels=0;
	int total_hooking_nodes=0;
	
	u_int *old_bnodes;
	u_char fnodes[MAXGROUPNODE];
	
	const char *ntop;

	/* Reset the hook */
	hook_reset();
	total_hooks++;
	
	/* 	
	  	* *	   The beginning          * *	  	
	 */
	loginfo("The hook begins. Starting to scan the area");

	/* 
	 * We do our first scans to know what we've around us. The rnodes are 
	 * kept in me.cur_node->r_nodes.
	 * The fastest one is in me.cur_node->r_nodes[0].
	 *
	 * If after MAX_FIRST_RADAR_SCANS# tries we haven't found any rnodes
	 * we start as a new gnode.
	 */
	
	for(i=0; i<MAX_FIRST_RADAR_SCANS; i++) {
		me.cur_node->flags|=MAP_HNODE;

		loginfo("Launching radar_scan %d of %d", i+1, MAX_FIRST_RADAR_SCANS);
		
		if(radar_scan(0))
			fatal("%s:%d: Scan of the area failed. Cannot continue.", 
					ERROR_POS);
		total_hooking_nodes=count_hooking_nodes();

		if(!me.cur_node->links || 
				( me.cur_node->links==total_hooking_nodes 
				  && !hook_retry )) {
			/* 
			 * If we have 0 nodes around us, we are alone, so we create a
			 * new gnode.
			 * If all the nodes around us are hooking and we started hooking
			 * before them, we create the new gnode.
			 */
			if(!me.cur_node->links) {
				/* 
				 * We haven't found any rnodes. Let's retry the
				 * radar_scan if i+1<MAX_FIRST_RADAR_SCANS
				 */
				if(i+1 < MAX_FIRST_RADAR_SCANS)
					goto hook_retry_scan;

				loginfo("No nodes found! This is a black zone. "
						"Creating a new_gnode.");
			} else
				loginfo("There are %d nodes around, which are hooking"
						" like us, but we came first so we have "
						"to create the new gnode", 
						total_hooking_nodes);
			create_gnodes(0, GET_LEVELS(my_family));
			ntop=inet_to_str(me.cur_ip);

			hook_set_all_ips(me.cur_ip, me.cur_ifs, me.cur_ifs_n);

			loginfo("Now we are in a brand new gnode. The ip %s is now"
					" used.", ntop);

			new_gnode=1;

			goto finish;
		} else if(hook_retry) {
			/* 
			 * There are only hooking nodes, but we started the hooking
			 * after them, so we wait until some of them create the new
			 * gnode.
			 */
			loginfo("I've seen %d hooking nodes around us, and one of them "
					"is becoming a new gnode.\n"
					"  We wait, then we'll restart the hook.", 
					total_hooking_nodes);

			usleep(rand_range(0, 1024)); /* ++entropy, thx to katolaz :) */
			sleep(MAX_RADAR_WAIT);
			i--;
		} else 
			break;

hook_retry_scan:
		reset_radar();
		rnode_destroy(me.cur_node);
		memset(me.cur_node, 0, sizeof(map_node));
		me.cur_node->flags|=MAP_HNODE;
		qspn_b_del_all_dead_rnodes();
	}

	loginfo("We have %d nodes around us. (%d are hooking)", 
			me.cur_node->links, total_hooking_nodes);

	/* 
	 * Now we choose the nearest rnode we found and we send it the 
	 * GET_FREE_NODES request.
	 */
	for(i=0, e=0; i<me.cur_node->links; i++) {
		if(!(rq=find_node_radar_q((map_node *)me.cur_node->r_node[i].r_node))) 
			fatal("%s:%d: This ultra fatal error goes against the "
					"laws of the universe. It's not "
					"possible!! Pray", ERROR_POS);
		if(rq->node->flags & MAP_HNODE)
			continue;

		if(!get_free_nodes(rq->ip, rq->dev, &fn_hdr, fnodes)) {
			/* Exctract the ipstart of the gnode */
			inet_setip(&gnode_ipstart, fn_hdr.ipstart, my_family);
			
			/* Get the qspn round infos */
			if(!get_qspn_round(rq->ip, rq->dev, rq->final_rtt,
						me.cur_qspn_time,
						me.cur_qspn_id,
						qspn_gnode_count)) {
				e=1;
				break;
			}
		}
	}	
	if(!e)
		fatal("None of the nodes in this area gave me the free_nodes info");

	/* 
	 * Let's choose a random ip using the free nodes list we received.
	 */

	e=rand_range(0, fn_hdr.nodes-1);
	if(fn_hdr.level == 1) {
		new_gnode=0;
		postoip(fnodes[e], gnode_ipstart, &me.cur_ip);
	} else {
		new_gnode=1;
		for(;;) {
			random_ip(&gnode_ipstart, fn_hdr.level, fn_hdr.gid, 
					GET_LEVELS(my_family), me.ext_map, 0, 
					&me.cur_ip, my_family);
			if(!inet_validate_ip(me.cur_ip))
				break;
		}
	}
	if(server_opt.restricted)
		inet_setip_localaddr(&me.cur_ip, my_family);
	hook_set_all_ips(me.cur_ip, me.cur_ifs, me.cur_ifs_n);

	/* 
	 * Fetch the ext_map from the node who gave us the free nodes list. 
	 */
	old_ext_map=me.ext_map;
	if(!(me.ext_map=get_ext_map(rq->ip, rq->dev, &me.cur_quadg))) 
		fatal("None of the rnodes in this area gave me the extern map");
	else
		free_extmap(old_ext_map, GET_LEVELS(my_family), 0);

	/* If we have to create new gnodes, let's do it. */
	if(new_gnode)
		create_gnodes(&me.cur_ip, fn_hdr.level);
	else {
		/* 
		 * We want a new shiny traslucent internal map 
		 */
		
		reset_int_map(me.int_map, 0);
		iptoquadg(me.cur_ip, me.ext_map, &me.cur_quadg, 
				QUADG_GID|QUADG_GNODE|QUADG_IPSTART);

		/* Increment the gnode seeds counter of level one, since
		 * we are new in that gnode */
		gnode_inc_seeds(&me.cur_quadg, 0);

		/* 
		 * Fetch the int_map from each rnode and merge them into a
		 * single, big, shiny map.
		 */
		imaps=0;
		rq=radar_q;
		merg_map=xmalloc(me.cur_node->links*sizeof(map_node *));
		memset(merg_map, 0, me.cur_node->links*sizeof(map_node *));

		for(i=0; i<me.cur_node->links; i++) {
			rq=find_node_radar_q((map_node *)me.cur_node->r_node[i].r_node);
			
			if(rq->node->flags & MAP_HNODE)
				continue;
			if(quadg_gids_cmp(rq->quadg, me.cur_quadg, 1)) 
				/* This node isn't part of our gnode, let's skip it */
				continue; 

			if((merg_map[imaps]=get_int_map(rq->ip, rq->dev, &new_root))) {
				merge_maps(me.int_map, merg_map[imaps], me.cur_node, new_root);
				imaps++;
			}
		}
		if(!imaps)
			fatal("None of the rnodes in this area gave me the int_map");
		
		for(i=0; i<imaps; i++)
			free_map(merg_map[i], 0);
		xfree(merg_map);
	}
	
	/* 
	 * Wow, the last step! Let's get the bnode map. Fast, fast, quick quick! 
	 */
	e=0;
	for(i=0; i<me.cur_node->links; i++) {
		rq=find_node_radar_q((map_node *)me.cur_node->r_node[i].r_node);
		if(rq->node->flags & MAP_HNODE)
			continue;
		if(quadg_gids_cmp(rq->quadg, me.cur_quadg, 1)) 
			/* This node isn't part of our gnode, let's skip it */
			continue; 
		old_bnode_map=me.bnode_map;	
		old_bnodes=me.bmap_nodes;
		me.bnode_map=get_bnode_map(rq->ip, rq->dev, &me.bmap_nodes);
		if(me.bnode_map) {
			bmap_levels_free(old_bnode_map, old_bnodes);
			e=1;
			break;
		} else {
			me.bnode_map=old_bnode_map;
			me.bmap_nodes=old_bnodes;
		}
	}
	if(!e)
		loginfo("None of the rnodes in this area gave me the bnode map.");
	
	if(free_the_tmp_cur_node) {
		free(me.cur_node);
		free_the_tmp_cur_node=0;
	}
	me.cur_node = &me.int_map[me.cur_quadg.gid[0]];
	map_node_del(me.cur_node);
	me.cur_node->flags &= ~MAP_VOID;
	me.cur_node->flags |= MAP_ME;
	/* We need a fresh me.cur_node */
	refresh_hook_root_node(); 
	
finish:
	/* 
	 * We must reset the radar_queue because the first radar_scan, used while hooking,
	 * has to keep the list of the rnodes' "inet_prefix ip". In this way we know
	 * the rnodes' ips even if we haven't an int_map yet.
	 */
	reset_radar();

	/* We have finished the hook */
	me.cur_node->flags&=~MAP_HNODE;

	if(new_gnode) {
		if(!me.cur_node->links)
			/* 
			 * We are a node lost in the desert, so we don't send
			 * anything because nobody is listening
			 */
			tracer_levels=0;
		else
			/* 
			 * We are a new gnode, so we send the tracer in all higher
			 * levels
			 */
			tracer_levels=fn_hdr.level;
	} else {
		/* 
		 * We are just a normal node inside a gnode, let's notice only
		 * the other nodes in this gnode.
		 */
		tracer_levels=2;
	}

	loginfo("Starting the second radar scan before sending our"
			" first tracer_pkt");
	if(radar_scan(0))
		fatal("%s:%d: Scan of the area failed. Cannot continue.", 
				ERROR_POS);
	
	/* 
	 * Now we send a simple tracer_pkt in all the level we have to. This pkt
	 * is just to say <<Hey there, I'm here, alive>>, thus the other nodes
	 * of the gnode will have the basic routes to reach us.
	 * Note that this is done only at the first time we hook.
	 */
	if(total_hooks == 1) {
		usleep(rand_range(0, 999999));
		tracer_pkt_start_mutex=0;
		for(i=1; i<tracer_levels; i++)
			tracer_pkt_start(i-1);
	}

	/* Let's fill the krnl routing table */
	loginfo("Filling the kernel routing table");
	rt_full_update(0);

	loginfo("Hook completed");

	return ret;
}

/*
 * And this is the end my dear.
 */
