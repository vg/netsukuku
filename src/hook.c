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

#include <sys/types.h>
#include <sys/time.h>
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ll_map.c"
#include "map.h"
#include "gmap.h"
#include "inet.h"
#include "radar.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"
#include "misc.h"

extern struct current me;
extern int my_family;

/*  *  *  put/get free_ips  *  *  */

/* get_free_ips: It send the GET_FREE_IPS request, used to retrieve the 
 * list of free/available IPs in the dst_node's gnode*/
int get_free_ips(inet_prefix to, struct free_ips *fi_hdr, int *ips)
{
	PACKET pkt, rpkt;
	char *ntop;
	ssize_t err;
	int ret=0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_FREE_IPS, 0, PUT_FREE_IPS, 1, &rpkt);
	if(err==-1) {
		error("get_free_ips(): Failed to send the GET_FREE_IPS request to %s. Skipping...", ntop);
		ret=-1;
		goto finish;
	}
	
	memcpy(fi_hdr, rpkt.msg, sizeof(struct free_ips));
	if(fi_hdr->ips <= 0 || fi_hdr->ips >= MAXGROUPNODE) {
		error("Malformed PUT_FREE_IPS request hdr. It says there are %d free ips", fi_hdr->ips);
		ret=-1;
		goto finish;
	}
	memcpy(ips, pkt.msg+sizeof(struct free_ips), fi_hdr->ips*sizeof(int));
	debug(DBG_NORMAL, "received %d free ips", fi_hdr->ips);

finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
}

/* put_free_ips: It generates the a list of free IPs available in the
 * cur_gnode.
 */
int put_free_ips(PACKET rq_pkt)
{
	/*I'm using this temp struct to do pkt.msg=&fipkt; see below*/
	struct fi_pkt {
		struct free_ips fi_hdr;
		int free_ips[MAXGROUPNODE];
	}fipkt;
	struct timeval cur_t;
	PACKET pkt;
	map_node *map=me.int_map;
	char *ntop; 
	ssize_t err, i, e=0;
	int ret;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_FREE_IPS reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, NULL);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	if(me.cur_gnode.g.flags & GMAP_FULL) {
		/*<<My gnode is full, sry>>*/
		pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_IPS, 0);
		err=pkt_err(pkt, E_GNODE_FULL);
	} else {
		fi_hdr.gid=me.cur_gid;
		memcpy(&fi_hdr.ipstart, &me.ipstart, sizeof(inet_prefix));
		/*Creates the list of our gnode's free ips. (it scans the int_map to find all the 
		 * MAP_VOID nodes)*/
		for(i=0; i<MAXGROUPNODE; i++)
			if(map[i].flags & MAP_VOID) {
				free_ips[e]=i;
				e++:
			}

		fi_hdr.ips=e;
		gettimeofday(&cur_t, 0);
		timersub(&cur_t, &me.cur_qspn_time, &fi_hdr.qtime);
		pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_FREE_IPS, sizeof(struct fi_pkt));
		pkt.msg=xmalloc(sizeof(struct fi_pkt));
		
		memcpy(pkt.msg, &fipkt, sizeof(struct fi_pkt));
		err=pkt_send(pkt);
	}
	
	if(err==-1) {
		error("put_free_ips(): Cannot send the PUT_FREE_IPS reply to %s.", ntop);
		ret=-1;
		goto finish;
	}
	
finish:	
	pkt_free(&pkt, 1);
	xfree(ntop);
	return 0;
}

/*  *  *  put/get ext_map  *  *  */

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
	if(rblock)
		xfree(rblock);
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/* get_ext_map: It sends the GET_EXT_MAP request to retrieve the 
 * dst_node's ext_map.
 */
map_gnode *get_ext_map(inet_prefix to, quadro_group *new_quadg)
{
	PACKET pkt, rpkt;
	char *ntop, *pack;
	int err;
	struct ext_map_hdr emap_hdr;
	map_rnode *rblock=0;
	map_gnode **ext_map=0, *ret=0;
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

/*  *  *  put/get int_map  *  *  */

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

	pkt.msg=pack_map(map, 0, MAXGROUPNODE, root_node, &pkt_sz);
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

/* get_int_map: It sends the GET_INT_MAP request to retrieve the 
 * dst_node's int_map. */
map_node *get_int_map(inet_prefix to, map_node *new_root)
{
	PACKET pkt, rpkt;
	char *ntop;
	int err;
	struct int_map_hdr imap_hdr;
	map_node *int_map, *ret=0;
	
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
	
	pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz):
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

/*  *  *  put/get bnode_map  *  *  */

int put_bnode_map(PACKET rq_pkt)
{
	PACKET pkt;
	map_bnode *bmap=me.bnode_map;
	char *ntop; 
	int ret;
	ssize_t err;
	size_t pkt_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_BNODE_MAP reply to %s", ntop);

	memset(&pkt, '\0', sizeof(PACKET));
	pkt.sk_type=SKT_TCP;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	pkt.msg=pack_map(bmap, me.cur_quadg.gnode[0], me.bmap_nodes, 0, &pkt_sz);
	pkt.hdr.sz=pkt_sz;
	err=send_rq(&pkt, 0, PUT_BNODE_MAP, rq_pkt.hdr.id, 0, 0, 0);
	if(err==-1) {
		error("put_bnode_maps(): Cannot send the PUT_BNODE_MAP reply to %s.", ntop);
		ret=-1;
		goto finish;
	}

finish:
	if(rblock)
		xfree(rblock);
	pkt_free(&pkt, 1);
	xfree(ntop);
	return ret;
}

/* get_bnode_map: It sends the GET_BNODE_MAP request to retrieve the 
 * dst_node's bnode_map.
 */
map_bnode *get_bnode_map(inet_prefix to, u_int *bmap_nodes)
{
	PACKET pkt, rpkt;
	char *ntop;
	int err;
	struct bnode_map_hdr imap_hdr;
	map_bnode *bnode_map, *ret=0;
	
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
	
	memcpy(&imap_hdr, rpkt.msg, sizeof(struct bnode_map_hdr));
	if(verify_int_map_hdr(&imap_hdr, MAXGROUPBNODE, MAXBNODE_RNODEBLOCK)) {
		error("Malformed PUT_BNODE_MAP request hdr.");
		ret=0;
		goto finish;
	}
	*bmap_nodes=imap_hdr.bnode_map_sz/sizeof(map_bnode);

	/*Extracting the map...*/
	pack_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz):
	pack=rpkt.msg;
	ret=bnode_map=unpack_map(pack, pack_sz, me.cur_quadg.gnode[0], 0, MAXGROUPBNODE, MAXBNODE_RNODEBLOCK);
	if(!bnode_map)
		error("get_bnode_map(): Malformed bnode_map. Cannot load it");
		
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
}

int create_gnode(void)
{
	int i;
	
	/* - scans the old ext_map;
	 * - Choose a random gnode excluding the one already used in the old map.
	 * - sit down and rest
	 */
	
	if(!me.ext_map) {
		/*We haven't an ext_map so let's cast the dice*/
		me.cur_gid=rand_range(0, /*LAST_GNODE(my_family)*/);
			/*TODO: gmap support: CONTINUE here*/
	} else {
		for(i=0; i<MAXGROUPNODE; i++) {
			/*TODO: gmap support: CONTINUE here*/
		}
	}
	return 0;
}

int netsukuku_hook(char *dev)
	
	int i, e=0, idx, imaps=0, ret=0;
	struct radar_queue *rq=radar_q;
	u_int idata[4];
	struct free_ips fi_hdr;
	int fips[MAXGROUPNODE];
	map_node **merg_map, *new_root;
	map_gnode *new_groot;

	srand(time(0));	
	debug(DBG_NOISE, "Starting the hooking");

	if(my_family==AF_INET)
		idata[0]=HOOKING_IP;
	else
		idata[0]=HOOKING_IP6;
	me.cur_ip.family=my_family;
	inet_setip(&me.cur_ip, idata, my_family);
	memcpy(&me.cur_ip, &me.cur_ip);
	
	/*We set the dev ip to HOOKING_IP to begin our transaction.*/
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the HOOKING_IP in %s", ERROR_POS, dev);
	
	if(route_add_df_gw(dev))
		fatal("%s:%d: Couldn't set the default gw for %s", ERROR_POS, dev);


		/* * * 		The beginning          * * */

	debug(DBG_NORMAL, "The hook begins. Starting to scan the area");
	me.cur_node->flags|=MAP_HNODE;
	
	/* We do our first scan to know what we've around us. The rnodes are kept in
	 * me.cur_node->r_nodes. The fastest one is in me.cur_node->r_nodes[0]
	 */
	if(radar_scan())
		fatal("%s:%d: Scan of the area failed. Cannot continue.", ERROR_POS);
	
	if(!me.cur_node->links) {
		loginfo("No nodes found! This is a black zone. Creating a new_gnode. W00t we're the first node");
		create_gnode();
		goto finish;
	}
	
	rq=radar_q;
	/*Now we choose the nearest rnode we found and we send it the GET_FREE_IPS request*/
	for(i=0; i<me.cur_node->links; i++) {
		if(!(rq=find_ip_radar_q(me.cur_node->r_node[i].r_node))) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");

		if(!get_free_ips(rq->ip, &fi_hdr, fips)) {
			/*We store as fast as possible the cur_qspn_time*/
			memcpy(&me.cur_qspn_time, &fi_hdr.qtime, sizeof(struct timeval));
			e=1;
			break;
		}
	}	
	if(!e)
		fatal("None of the nodes in this area gave me the free_ips info");

	/*Now we choose a random ip from the free ips we received*/
	e=rand_range(0, fi_hdr.ips);
	maptoip(0, fips[e], fi_hdr.ipstart, &me.cur_ip);
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the tmp_ip in %s", ERROR_POS, dev);
		
	/*Fetch the ext_map from the nearest rnode*/
	e=0;
	rq=radar_q;
	for(i=0; i<me.cur_node->links; i++) {
		rq=find_ip_radar_q(me.cur_node->r_node[i].r_node);
		/*TODO: CONTINUE HERE: ................. \/ \/ */
		if(!(me.ext_map=get_ext_map(rq->ip, &new_quadg))) {
			e=1;
			break;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the extern map");
	/*Now we are ufficially in the fi_hdr.gid gnode*/
	new_groot->g.flags&=~GMAP_ME;
	me.cur_gid=gid;
	me.cur_gnode=gnode_from_pos(gid, me.ext_map[/*XXX: _EL(level)*/]);
	me.cur_gnode->g.flags|=GMAP_ME;
	/*TODO: iptoquadg();*/
	memcpy(&me.ipstart, &fi_hdr.ipstart, sizeof(inet_prefix));
	
	/*Fetch the int_map from each rnode*/
	imaps=0;
	rq=radar_q;
	merg_map=xmalloc(me.cur_node->links*sizeof(map_node *));
	memset(merg_map, 0, me.cur_node->links*sizeof(map_node *));
	for(i=0; i<me.cur_node->links; i++) {
		rq=find_ip_radar_q(me.cur_node->r_node[i].r_node);
		if(iptogid(rq->ip, 0) != me.cur_gid)
			/*This node isn't part of our gnode, let's skip it*/
			continue; 
			
		if((merg_map[imaps]=get_int_map(rq->ip, &new_root))) {
			merge_maps(me.int_map, merg_map[imaps], me.cur_node, new_root);
			imaps++;
		}
	}
	if(!imaps)
		fatal("None of the rnodes in this area gave me the intern map");

	for(i=0, i<imaps; i++)
		free_map(merg_map[i], 0);
	xfree(merg_map);

	/*Wow, the last step! Let's get the bnode map. Fast, fast, quick quick!*/
	e=0;
	for(i=0; i<me.cur_node->links; i++) {
		rq=find_ip_radar_q(me.cur_node->r_node[i].r_node);
		if(iptogid(rq->ip, 0) != me.cur_gid)
			/*This node isn't part of our gnode, let's skip it*/
			continue; 
			
		if(!me.bnode_map=get_bnode_map(rq->ip, &me.cur_bnode)) {
			e=1;
			break;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the bnode map. (Bastards!)");


finish:
	/* We must reset the radar_queue because the first radar_scan used while hooking
	 * has to keep the list of the rnodes' "inet_prefix ip". In this way we know
	 * the rnodes' ips even if we haven't an int_map yet.
	 */
	reset_radar(me.cur_node->links);
	me.cur_node->flags&=~MAP_HNODE;

	/*TODO: for now the gnodes aren't supported
	 * djkstra(ext_map);
	 */

	tracer_pkt_start();	/*<<Ehi there, I'm here, alive.>>. This is out firt tracer_pkt, we must
				  give to the other nodes the basic routes to reach us.*/
	
	/* I think I will abort the use of dnode, cuz the qspn rox enough
	 * dnode_set_ip(rnd(info.free_ips);
	 * QSPN_SEND_DNODE_GATHERING();
	 * snode_transform():
	 */
	return ret;
}

/* Action lines to trasform a dnode in a snode
int snode_transfrom(void)
{
	wait(DNODE_TO_SNODE_WAIT);
	local_broadcast_send(I_AM_A_SNODE);
	rcv(OK_FROM_RNODES);
	QSPN_SEND();
}
*/
