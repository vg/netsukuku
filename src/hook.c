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
#include <pthread.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#include "ll_map.c"
#include "map.h"
#include "gmap.h"
#include "inet.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

extern struct current me;
extern int my_family;

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
	PACKET pkt;
	map_node *map=me.int_map;
	char *ntop; 
	ssize_t err, i, e=0;
	int ret;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_FREE_IPS reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	to.family=my_family;
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
				free_ips[e]=(map[i]-me.int_map)/sizeof(map_node);
				e++:
			}

		fi_hdr.ips=e;
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
	map_gnode *map=me.ext_map;
	map_rnode *rblock=0;
	struct ext_map_hdr emap_hdr;
	char *ntop; 
	int count, ret;
	ssize_t err, pkt_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_EXT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	to.family=my_family;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, NULL);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	rblock=gmap_get_rblock(map, &count);
	emap_hdr.root_node=(me.cur_gnode-me.ext_map)/sizeof(map_gnode);
	emap_hdr.rblock_sz=count*sizeof(map_rnode);
	emap_hdr.ext_map_sz=MAXGROUPNODE*sizeof(map_gnode);
	pkt_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.rblock_sz):

	pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_EXT_MAP, pkt_sz);
	pkt.msg=xmalloc(pkt_sz);
	memcpy(pkt.msg, &emap_hdr, sizeof(struct ext_map_hdr));
	memcpy(pkt.msg+sizeof(struct ext_map_hdr), map, emap_hdr.ext_map_sz);
	memcpy(pkt.msg+sizeof(struct ext_map_hdr)+emap_hdr.ext_map_sz, rblock, emap_hdr.rblock_sz);

	err=pkt_send(pkt);
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
int get_ext_map(inet_prefix to, map_gnode *ext_map, map_gnode *new_root)
{
	PACKET pkt, rpkt;
	char *ntop;
	int ret=0, err;
	struct ext_map_hdr emap_hdr;
	map_rnode *rblock=0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_EXT_MAP, 0, PUT_EXT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=-1;
		goto finish;
	}
	
	memcpy(&emap_hdr, rpkt.msg, sizeof(struct ext_map_hdr));
	if(emap_hdr.rblock_sz > MAXROUTES*sizeof(map_rnode) ||
			emap_hdr.ext_map_sz > MAXGROUPNODE*sizeof(map_gnode)) {
		error("Malformed PUT_EXT_MAP request hdr.");
		ret=-1;
		goto finish;
	}
		
	/*Extracting the map...*/
	memcpy(ext_map, rpkt.msg+sizeof(struct ext_map_hdr), emap_hdr.ext_map_sz);
	
	/*Extracting the rnodes block and merging it to the map*/
	rblock=rpkt.msg+sizeof(struct ext_map_hdr)+emap_hdr.ext_map_sz;
	err=gmap_store_rblock(ext_map, rblock, emap_hdr.rblock_sz/sizeof(map_rnode));
	if(err!=emap_hdr.rblock_sz) {
		error("An error occurred while storing the rnodes block in the ext_map");
		ret=-1;
		goto finish;
	}
	new_root=(map_node *)(ext_map+(emap_hdr.root_node*sizeof(map_gnode)));
	new_root->g.flags|=GMAP_ME;
	
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
	map_rnode *rblock=0;
	struct int_map_hdr imap_hdr;
	char *ntop; 
	int count, ret;
	ssize_t err, pkt_sz=0;
	
	ntop=inet_to_str(&rq_pkt.from);
	debug(DBG_NORMAL, "Sending the PUT_INT_MAP reply to %s", ntop);
	
	memset(&pkt, '\0', sizeof(PACKET));
	to.family=my_family;
	pkt_addto(&pkt, &rq_pkt.from);
	pkt_addport(&pkt, ntk_tcp_port);
	pkt_addflags(&pkt, NULL);
	pkt_addsk(&pkt, rq_pkt.sk, rq_pkt.sk_type);

	rblock=map_get_rblock(map, &count);
	imap_hdr.root_node=(me.cur_node-me.int_map)/sizeof(map_node);
	imap_hdr.rblock_sz=count*sizeof(map_rnode);
	imap_hdr.int_map_sz=MAXGROUPNODE*sizeof(map_node);
	pkt_sz=INT_MAP_BLOCK_SZ(imap_hdr.int_map_sz, imap_hdr.rblock_sz):

	pkt_fill_hdr(&pkt.hdr, rq_pkt.hdr.id, PUT_INT_MAP, pkt_sz);
	pkt.msg=xmalloc(pkt_sz);
	memcpy(pkt.msg, &imap_hdr, sizeof(struct int_map_hdr));
	memcpy(pkt.msg+sizeof(struct int_map_hdr), map, imap_hdr.int_map_sz);
	memcpy(pkt.msg+sizeof(struct int_map_hdr)+imap_hdr.int_map_sz, rblock, imap_hdr.rblock_sz);

	err=pkt_send(pkt);
	if(err==-1) {
		error("put_int_maps(): Cannot send the PUT_INT_MAP reply to %s.", ntop);
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

/* get_int_map: It sends the GET_INT_MAP request to retrieve the 
 * dst_node's int_map.
 */
int get_int_map(inet_prefix to, map_node *int_map, map_node *new_root)
{
	PACKET pkt, rpkt;
	char *ntop;
	int ret=0, err;
	struct int_map_hdr imap_hdr;
	map_rnode *rblock=0;
	
	memset(&pkt, '\0', sizeof(PACKET));
	memset(&rpkt, '\0', sizeof(PACKET));
	
	ntop=inet_to_str(&to);
	
	pkt_addto(&pkt, &to);
	pkt.sk_type=SKT_TCP;
	err=send_rq(&pkt, 0, GET_INT_MAP, 0, PUT_INT_MAP, 1, &rpkt);
	if(err==-1) {
		ret=-1;
		goto finish;
	}
	
	memcpy(&imap_hdr, rpkt.msg, sizeof(struct int_map_hdr));
	if(imap_hdr.rblock_sz > MAXROUTES*sizeof(map_rnode) ||
			imap_hdr.int_map_sz > MAXGROUPNODE*sizeof(map_node)) {
		error("Malformed PUT_INT_MAP request hdr.");
		ret=-1;
		goto finish;
	}
		
	/*Extracting the map...*/
	memcpy(int_map, rpkt.msg+sizeof(struct int_map_hdr), imap_hdr.int_map_sz);
	
	/*Extracting the rnodes block and merging it to the map*/
	rblock=rpkt.msg+sizeof(struct int_map_hdr)+imap_hdr.int_map_sz;
	err=map_store_rblock(int_map, rblock, imap_hdr.rblock_sz/sizeof(map_rnode));
	if(err!=imap_hdr.rblock_sz) {
		error("An error occurred while storing the rnodes block in the int_map");
		ret=-1;
		goto finish;
	}
	new_root=(map_node *)(int_map+(imap_hdr.root_node*sizeof(map_node)));
	new_root->flags|=MAP_ME;
	
	/*Finished, yeah*/
finish:
	pkt_free(&pkt, 0);
	pkt_free(&rpkt, 1);
        xfree(ntop);
	return ret;
}

int netsukuku_hook(char *dev)
{
	int i, e=0, idx, imaps=0, ret=0;
	u_int idata[4];
	struct free_ips fi_hdr;
	int fips[MAXGROUPNODE];
	map_node **merg_map, *new_root;
	map_gnode *new_groot;

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
	
	debug(DBG_NORMAL, "The hook begins. Starting to scan the area");
	me.cur_node->flags|=MAP_HNODE;
	
	/* We do our first scan to know what we've around us. The rnodes are kept in
	 * me.cur_node->r_nodes. The fastest one is in me.cur_node->r_nodes[0]
	 */
	if(radar_scan())
		fatal("%s:%d: Scan of the area failed. Cannot continue.", ERROR_POS);
	
	if(!me.cur_node->links) {
		/*TODO:
		 * create_new_gnode() {
		 * 	1) scans the old ext_map;
		 * 	2) Choose a random gnode excluding the one already used in the old map.
		 * 	3) sit down and rest
		 * }
		 */
		loginfo("No nodes founded! This is a black zone. Creating a new_gnode. W00t we're the first node");
		goto finish;
	}
	
	/*Now we choose the nearest rnode we found and we send it the GET_FREE_IPS request*/
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");

		if(!get_free_ips(radar_q[idx].ip, &fi_hdr, fips)) {
			e=1;
			break;
		}
	}	
	if(!e)
		fatal("None of the nodes in this area gave me the free_ips info");

	/*Now we choose a random ip from the free ips we received*/
	e=(rand()%(fi_hdr.ips-0+1))+0;		/*rnd_range algo: (rand()%(max-min+1))+min*/
	maptoip(0, fips[e], fi_hdr.ipstart, &me.cur_ip);
	if(set_dev_ip(me.cur_ip, dev))
		fatal("%s:%d: Cannot set the tmp_ip in %s", ERROR_POS, dev);
		
	/*Fetch the ext_map from the nearest rnode*/
	e=0;
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");
		
		if(!get_ext_map(radar_q[idx].ip, &me.ext_mapm, &new_groot)) {
			e=1;
			break;
		}
	}
	if(!e)
		fatal("None of the rnodes in this area gave me the extern map");
	/*Now we are ufficially in the fi_hdr.gid gnode*/
	new_groot->g.flags&=~GMAP_ME;
	set_cur_gnode(fi_hdr.gid);
	
	/*Fetch the int_map from each rnode*/
	imaps=0;
	merg_map=xmalloc(me.cur_node->links*sizeof(map_node *));
	memset(merg_map, 0, me.cur_node->links*sizeof(map_node *));
	for(i=0; i<me.cur_node->links; i++) {
		if((idx=find_ip_radar_q(me.cur_node->r_node[i].r_node)==-1)) 
			fatal("%s:%d: This ultra fatal error goes against the laws of the universe. It's not possible!! Pray");
		
		if(iptogid(radar_q[idx].ip) != me.cur_gid)
			/*This node isn't part of our gnode, let's skip it*/
			continue; 
			
		if(!get_int_map(radar_q[idx].ip, merg_map[imaps], &new_root)) {
			*(merg_map+i)=xmalloc(sizeof(map_node)*MAXGROUPNODE);
			merge_maps(me.int_map, merg_map[i], me.cur_node, new_root);
			imaps++;
		}
	}
	if(!imaps)
		fatal("None of the rnodes in this area gave me the intern map");

	for(i=0, i<imaps; i++)
		xfree(merg_map[i]);
	xfree(merg_map);

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
	
	/* I think that i will abort the use of dnode, cuz the qspn rox enough
	 * dnode_set_ip(rnd(info.free_ips);
	 * QSPN_SEND_DNODE_GATHERING();
	 * snode_transform():
	 */
	return ret;
}

/*
 * Action lines to trasform a dnode in a snode
int snode_transfrom(void)
{
	wait(DNODE_TO_SNODE_WAIT);
	local_broadcast_send(I_AM_A_SNODE);
	rcv(OK_FROM_RNODES);
	QSPN_SEND();
}
*/
