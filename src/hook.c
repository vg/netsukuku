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
#include "ll_map.h"
#include "inet.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "pkts.h"
#include "tracer.h"
#include "hook.h"
#include "radar.h"
#include "netsukuku.h"
#include "request.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"

/*  
 *  *  *  put/get free_nodes  *  *  *
 */

/* 
 * get_free_nodes: It send the GET_FREE_NODES request, used to retrieve the free_nodes
 * pkt (see hook.h).
 */
int get_free_nodes(inet_prefix to, struct free_nodes_hdr *fn_hdr, int *nodes, struct timeval *qtime)
{
	PACKET pkt, rpkt;
	char *ntop;
	ssize_t err;
	int ret=0, buf=0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_FREE_NODES, 0, PUT_FREE_NODES, 1, &rpkt);
	if(err==-1) {
		error("get_free_nodes(): Failed to send the GET_FREE_NODES request to %s. "
				"Skipping...", ntop);
		ret=-1;
		goto finish;
	}
	
	memcpy(fn_hdr, rpkt.msg, sizeof(struct free_nodes_hdr));
	if(fn_hdr->nodes <= 0 || fn_hdr->nodes >= MAXGROUPNODE || 
			fn_hdr->max_levels >= me.cur_quadg.levels || !fn_hdr->level
			|| fn_hdr->level >= fn_hdr->max_levels) {
		error("Malformed PUT_FREE_NODES request hdr from %s.", ntop);
		ret=-1;
		goto finish;
	}
	
	buf=sizeof(struct free_nodes_hdr);
	memcpy(qtime, pkt.msg+buf, fn_hdr->max_levels * sizeof(struct timeval));
	
	buf+=fn_hdr->max_levels * sizeof(struct timeval);
	memcpy(nodes, pkt.msg+buf, fn_hdr->nodes * sizeof(int));

	debug(DBG_NORMAL, "Received %d free %s", fn_hdr->nodes, 
			fn_hdr->level == 1 ? "nodes" : "gnodes");
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
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
		struct timeval  qtime[me.cur_quadg.levels];

		/* 
		 * This int free_nodes has maximum MAXGROUPNODE elements, so in
		 * the pkt it will be truncated to free_nodes[fn_hdr.nodes]
		 * elements */
		int free_nodes[MAXGROUPNODE];
	}fn_pkt;

	PACKET pkt;
	struct timeval cur_t;
	map_node *map;
	
	int ret=0, i, e=0;
	u_char level;
	ssize_t err, pkt_sz;
	char *ntop; 
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_FREE_NODES reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, 0);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);


	/* We search in each level an our gnode which is not full. */
	e=0;
	for(level=1; level < me.cur_quadg.levels; level++)
		if(!(me.cur_quadg.gnode[_EL(level)]->g.flags & GMAP_FULL)) {
			e=1;
			break;
		}
	if(!e) {
		/* <<My quadro_group is completely full, sry>> */
		pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_NODES, 0);
		err=pkt_err(pkt, E_QGROUP_FULL);
		goto finish;
	}

	/* Ok, we've found one, so let's roll the pkt */
	fn_pkt.fn_hdr.max_levels=me.cur_quadg.levels;
	memcpy(&fn_pkt.fn_hdr.ipstart, &me.cur_quadg.ipstart[level], sizeof(inet_prefix));
	fn_pkt.fn_hdr.level=level;
	fn_pkt.fn_hdr.gid=me.cur_quadg.gid[level];
	
	/*
	 * Creates the list of our gnode's free nodes. If the gnode level is 1 it
	 * scans the int_map to find all the MAP_VOID nodes, otherwise it scans
	 * the gnode map at level-1.
	 */
	e=0;
	if(level == 1) {
		map=me.int_map;
		for(i=0; i<MAXGROUPNODE; i++)
		if(map[i].flags & MAP_VOID) {
			fn_pkt.free_nodes[e]=i;
			e++;
		}

	} else {
		for(i=0; i<MAXGROUPNODE; i++)
		if(me.ext_map[_EL(level-1)][i].flags & GMAP_VOID ||
				me.ext_map[_EL(level-1)][i].g.flags & MAP_VOID) {
			fn_pkt.free_nodes[e]=i;
			e++;
		}
	}
	fn_pkt.fn_hdr.nodes=e;
	
	/* We fill the qspn round time */
	gettimeofday(&cur_t, 0);
	for(level=1; level < me.cur_quadg.levels; level++)
		timersub(&cur_t, &me.cur_qspn_time[level], &fn_pkt.qtime[level]);

	/* Go pkt, go! Follow your instinct */
	pkt_sz=FREE_NODES_SZ(fn_pkt.fn_hdr.max_levels, fn_pkt.fn_hdr.nodes);
	pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_NODES, pkt_sz); 
	pkt.msg=xmalloc(pkt_sz);
	memcpy(pkt.msg, &fn_pkt, pkt_sz);
	err=pkt_send(&pkt);
	
finish:	
	if(err==-1) {
		error("put_free_nodes(): Cannot send the PUT_FREE_NODES reply to %s.", ntop);
		ret=-1;
	}
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/*  
 *  *  *  put/get ext_map  *  *  *
 */

int put_ext_map(PACKET rq_pkt)
{
	PACKET pkt;
	struct ext_map_hdr emap_hdr;
	char *ntop; 
	int ret=0;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_EXT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_extmap(me.ext_map, MAXGROUPNODE, &me.cur_quadg, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	err=send_rq(&pkt, 0, PUT_EXT_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_ext_maps(): Cannot send the PUT_EXT_MAP reply to %s.", ntop);
		ret=-1;
		goto finish;
	}

finish:
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/* 
 * get_ext_map: It sends the GET_EXT_MAP request to retrieve the
 * dst_node's ext_map.
 */
map_gnode **get_ext_map(inet_prefix to, quadro_group *new_quadg)
{
	PACKET pkt, rpkt;
	char *ntop, *pack;
	int err;
	struct ext_map_hdr emap_hdr;
	map_rnode *rblock=0;
	map_gnode **ext_map=0, **ret=0;
	size_t pack_sz;

	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_EXT_MAP, 0, PUT_EXT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	memcpy(&emap_hdr, rpkt.msg, sizeof(struct ext_map_hdr));
	if(verify_ext_map_hdr(&emap_hdr)) {
		error("Malformed PUT_EXT_MAP request hdr.");
		ret=0;
		goto finish;
	}

	pack_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.total_rblock_sz);
	pack=rpkt.msg;
	ret=ext_map=unpack_extmap(pack, pack_sz, new_quadg);
	if(!ext_map)
		error("get_ext_map: Malformed ext_map. Cannot unpack the ext_map.");
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
}

/*  
 *  *  *  put/get int_map  *  *  *
 */

int put_int_map(PACKET rq_pkt)
{
	PACKET pkt;
	map_node *map=me.int_map;
	char *ntop; 
	int count, ret;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_INT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt.sk_type=SKT_TCP;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_map(map, 0, MAXGROUPNODE, me.cur_node, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	err=send_rq(&pkt, 0, PUT_INT_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_int_maps(): Cannot send the PUT_INT_MAP reply to %s.", ntop);
		ret=-1;
		goto finish;
	}
finish:
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/* 
 * get_int_map: It sends the GET_INT_MAP request to retrieve the 
 * dst_node's int_map. 
 */
map_node *get_int_map(inet_prefix to, map_node **new_root)
{
	PACKET pkt, rpkt;
	struct int_map_hdr imap_hdr;
	map_node *int_map, *ret=0;
	size_t pack_sz;
	int err;
	char *pack, *ntop;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_INT_MAP, 0, PUT_INT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	memcpy(&imap_hdr, rpkt.msg, sizeof(struct int_map_hdr));
	if(verify_int_map_hdr(&imap_hdr, MAXGROUPNODE, MAXRNODEBLOCK)) {
		error("Malformed PUT_INT_MAP request hdr.");
		ret=0;
		goto finish;
	}
	
	pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz);
	pack=rpkt.msg;
	ret=int_map=unpack_map(pack, pack_sz, 0, new_root, MAXGROUPNODE, MAXRNODEBLOCK);
	if(!int_map)
		error("get_int_map(): Malformed int_map. Cannot load it");
	
	/*Finished, yeah*/
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
}

/*  
 *  *  *  put/get bnode_map  *  *  *
 */

int put_bnode_map(PACKET rq_pkt)
{
	PACKET pkt;
	map_bnode **bmaps=me.bnode_map;
	char *ntop; 
	int ret;
	ssize_t err;
	size_t pack_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_BNODE_MAP reply to %s", ntop);

	memset(&pkt, '\0', sizeof(PACKET));
	pkt.sk_type=SKT_TCP;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_all_bmaps(bmaps, me.bmap_nodes, me.ext_map, me.cur_quadg, &pack_sz);
	pkt.hdr.sz=pack_sz;
	err=send_rq(&pkt, 0, PUT_BNODE_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_bnode_maps(): Cannot send the PUT_BNODE_MAP reply to %s.", ntop);
		ret=-1;
		goto finish;
	}

finish:
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/* 
 * get_bnode_map: It sends the GET_BNODE_MAP request to retrieve the 
 * dst_node's bnode_map. 
 */
map_bnode **get_bnode_map(inet_prefix to, u_int **bmap_nodes)
{
	PACKET pkt, rpkt;
	int err;
	struct bmaps_hdr bmaps_hdr;
	map_bnode **bnode_map, **ret=0;
	size_t pack_sz;
	u_char levels;
	char *ntop, *pack;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_BNODE_MAP, 0, PUT_BNODE_MAP, 1, &rpkt);
	if(err==-1) {
		ret=0;
		goto finish;
	}
	
	memcpy(&bmaps_hdr, rpkt.msg, sizeof(struct bmaps_hdr));
	levels=bmaps_hdr.levels;
	pack_sz=bmaps_hdr.bmaps_block_sz;
	if(levels > GET_LEVELS(my_family) || pack_sz < sizeof(struct bnode_map_hdr)) {
		error("Malformed PUT_BNODE_MAP request hdr.");
		ret=0;
		goto finish;
	}

	/* Extracting the map... */
	pack=rpkt.msg+sizeof(struct bmaps_hdr);
	ret=bnode_map=unpack_all_bmaps(pack, pack_sz, levels, me.ext_map, bmap_nodes, 
			MAXGROUPNODE, MAXBNODE_RNODEBLOCK);
	if(!bnode_map)
		error("get_bnode_map(): Malformed bnode_map. Cannot load it");

finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
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

	if(!ip)
		random_ip(0, 0, 0, GET_LEVELS(my_family), me.ext_map, 0, 
				&me.cur_ip, my_family);
	else
		memcpy(&me.cur_ip, ip, sizeof(inet_prefix));

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
	reset_extmap(me.ext_map, final_level, 0);

	/* Now, we update the ext_map with the new gnodes */
	me.cur_quadg.levels=GET_LEVELS(my_family);
	iptoquadg(me.cur_ip, me.ext_map, &me.cur_quadg, QUADG_GID | QUADG_GNODE | QUADG_IPSTART);

	for(i=1; i<final_level; i++) {
		me.cur_quadg.gnode[_EL(i)]->flags &= ~GMAP_VOID;
		me.cur_quadg.gnode[_EL(i)]->flags |=  GMAP_ME;
		me.cur_quadg.gnode[_EL(i)]->g.flags &~ MAP_VOID;
		me.cur_quadg.gnode[_EL(i)]->g.flags |= MAP_ME | MAP_GNODE;
	}

	return 0;
}

int netsukuku_hook(char *dev)
{	
	struct radar_queue *rq=radar_q;
	struct free_nodes_hdr fn_hdr;
	map_node **merg_map, *new_root;
	map_gnode *new_groot, **old_ext_map;
	map_bnode **old_bnode_map;	
	inet_prefix new_ip;
	int i, e=0, idx, imaps=0, ret=0, new_gnode=0, tracer_levels=0;
	int fnodes[MAXGROUPNODE], *old_bnodes;
	u_int idata[4];
	char *ntop;

	/* We set the dev ip to HOOKING_IP to begin our transaction. */
	memset(idata, 0, sizeof(int)*4);
	if(my_family==AF_INET) 
		idata[0]=HOOKING_IP;
	else
		idata[0]=HOOKING_IP6;
	idata[0]=htonl(idata[0]);
	me.cur_ip.family=my_family;
	inet_setip(&me.cur_ip, idata, my_family);

	ntop=inet_to_str(&me.cur_ip);
	debug(DBG_NORMAL, "Setting the %s ip to %s interface", ntop, dev);
	xfree(ntop);
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the HOOKING_IP in %s", ERROR_POS, dev);
	
	if(rt_add_def_gw(dev))
		debug(DBG_NORMAL, "%s:%d: Couldn't set the default gw for %s", 
				ERROR_POS, dev);


	/* 	
	  	* * 		The beginning          * *	  	
	 */
	loginfo("The hook begins. Starting to scan the area");
	
	/* We use a fake root_node for a while */
	me.cur_node=xmalloc(sizeof(map_node));	/*TODO: WARNING XFREE it later!!*/
	memset(me.cur_node, 0, sizeof(map_node));
	me.cur_node->flags|=MAP_HNODE;
	
	/* 
	 * We do our first scan to know what we've around us. The rnodes are kept in
	 * me.cur_node->r_nodes. The fastest one is in me.cur_node->r_nodes[0]
	 */
	if(radar_scan())
		fatal("%s:%d: Scan of the area failed. Cannot continue.", 
				ERROR_POS);

	if(!me.cur_node->links) {
		loginfo("No nodes found! This is a black zone. "
				"Creating a new_gnode. W00t we're the first node");
		create_gnodes(0, GET_LEVELS(my_family));
		ntop=inet_to_str(&me.cur_ip);
		if(set_dev_ip(me.cur_ip, dev))
			fatal("%s:%d: Cannot set the new ip in %s", ERROR_POS, dev);
		loginfo("Now we are in a brand new gnode. The ip of %s is set "
				"to %s", dev, ntop);
		xfree(ntop);
		new_gnode=1;
		goto finish;
	}

	/* 
	 * Now we choose the nearest rnode we found and we send it the 
	 * GET_FREE_NODES request.
	 */
	rq=radar_q;
	for(i=0; i<me.cur_node->links; i++) {
		if(!(rq=find_ip_radar_q((map_node *)me.cur_node->r_node[i].r_node))) 
			fatal("%s:%d: This ultra fatal error goes against the "
					"laws of the universe. It's not "
					"possible!! Pray");

		if(!get_free_nodes(rq->ip, &fn_hdr, fnodes, me.cur_qspn_time)) {
			e=1;
			break;
		}
	}	
	if(!e)
		fatal("None of the nodes in this area gave me the free_nodes info");

	/* 
	 * Let's choose a random ip using the free nodes list we received.
	 */
	e=rand_range(0, fn_hdr.nodes);
	if(fn_hdr.level == 1) {
		new_gnode=0;
		postoip(fnodes[e], fn_hdr.ipstart, &me.cur_ip);
	} else {
		new_gnode=1;
		random_ip(&fn_hdr.ipstart, fn_hdr.level, fn_hdr.gid, 
				GET_LEVELS(my_family), me.ext_map, 0, 
				&me.cur_ip, my_family);
	}
	
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the tmp ip in %s", ERROR_POS, dev);

	/* 
	 * Fetch the ext_map from the rnode who gave us the free nodes list. 
	 */
	old_ext_map=me.ext_map;
	if(me.ext_map=get_ext_map(rq->ip, &me.cur_quadg)) 
		fatal("None of the rnodes in this area gave me the extern map");
	else {
		free_extmap(old_ext_map, GET_LEVELS(my_family), 0);
	}

	/* If we have to create new gnodes, let's do it. */
	if(new_gnode)
		create_gnodes(&me.cur_ip, fn_hdr.level);
	else {
		/* 
		 * Fetch the int_map from each rnode and merge them into a
		 * single, big, shiny map.
		 */
		imaps=0;
		rq=radar_q;
		merg_map=xmalloc(me.cur_node->links*sizeof(map_node *));
		memset(merg_map, 0, me.cur_node->links*sizeof(map_node *));
		for(i=0; i<me.cur_node->links; i++) {
			rq=find_ip_radar_q((map_node *)me.cur_node->r_node[i].r_node);
			
			if(quadg_diff_gids(rq->quadg, me.cur_quadg)) 
				/* This node isn't part of our gnode, let's skip it */
				continue; 

			if((merg_map[imaps]=get_int_map(rq->ip, &new_root))) {
				merge_maps(me.int_map, merg_map[imaps], me.cur_node, new_root);
				imaps++;
			}
		}
		if(!imaps)
			fatal("None of the rnodes in this area gave me the intern map");
		
		for(i=0; i<imaps; i++)
			free_map(merg_map[i], 0);
		xfree(merg_map);
	}
	
	/* 
	 * Wow, the last step! Let's get the bnode map. Fast, fast, quick quick! 
	 */
	e=0;
	for(i=0; i<me.cur_node->links; i++) {
		rq=find_ip_radar_q((map_node *)me.cur_node->r_node[i].r_node);
		if(quadg_diff_gids(rq->quadg, me.cur_quadg)) 
			/* This node isn't part of our gnode, let's skip it */
			continue; 
		old_bnode_map=me.bnode_map;	
		old_bnodes=me.bmap_nodes;
		me.bnode_map=get_bnode_map(rq->ip, &me.bmap_nodes);
		if(!me.bnode_map) {
			bmap_level_free(old_bnode_map, old_bnodes);
			e=1;
			break;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the bnode map. (Bastards!)");

finish:
	/* 
	 * We must reset the radar_queue because the first radar_scan, used while hooking,
	 * has to keep the list of the rnodes' "inet_prefix ip". In this way we know
	 * the rnodes' ips even if we haven't an int_map yet.
	 */
	reset_radar();
	me.cur_node->flags&=~MAP_HNODE;

	/* 
	 * <<Hey there, I'm here, alive>>. We send our firt tracer_pkt, we
	 * must give to the other nodes the basic routes to reach us.
	 */
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

	tracer_pkt_start_mutex=0;
	for(i=1; i<tracer_levels; i++)
		tracer_pkt_start(i);
	
	return ret;
}
