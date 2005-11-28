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
 */

#include "includes.h"

#include "misc.h"
#include "request.h"
#include "pkts.h"
#include "bmap.h"
#include "radar.h"
#include "route.h"
#include "radar.h"
#include "rehook.h"
#include "tracer.h"
#include "qspn.h"
#include "netsukuku.h"
#include "xmalloc.h"
#include "log.h"

/* 
 * ip_to_rfrom: If `rip_quadg' is null, it converts the `rip' ip in a 
 * quadro_group that is stored in `new_quadg' (if it is not null), otherwise it
 * uses `rip_quadg' itself.
 * The flags passed to iptoquadg are orred whith `quadg_flags'. 
 * The rnode position of the root_node of level 0 which corresponds to 
 * the given ip is returned, if it isn't found -1 is returned.
 */
int ip_to_rfrom(inet_prefix rip, quadro_group *rip_quadg,
		quadro_group *new_quadg, char quadg_flags)
{
	quadro_group qdg, *quadg;
	map_node *from;
	ext_rnode_cache *erc;
	int ret, external_node=0;

	quadg=&qdg;

	if(rip_quadg) {
		quadg=rip_quadg;
	} else {
		quadg_flags|=QUADG_GID|QUADG_GNODE;		
		iptoquadg(rip, me.ext_map, quadg, quadg_flags);
		if(new_quadg)
			memcpy(new_quadg, quadg, sizeof(quadro_group));
	}
	
	if(quadg_gids_cmp(me.cur_quadg, *quadg, 1))
		external_node=1;
	
	if(!external_node) {
		iptomap((u_int)me.int_map, rip, me.cur_quadg.ipstart[1], &from);
		ret=rnode_find(me.cur_node, from);
	} else {
		erc=e_rnode_find(me.cur_erc, quadg, 0);
		ret = !erc ? -1 : erc->rnode_pos;
	}
	
	return ret;
}

/* 
 * tracer_verify_pkt: It checks the validity of `tracer': The last entry
 * in the tracer must be a node present in our r_nodes.
 * Instead of using iptoquadg it uses `rip_quadg' if it isn't null.
 */
int tracer_verify_pkt(tracer_chunk *tracer, u_short hops, inet_prefix rip, 
		quadro_group *rip_quadg, int level)
{
	quadro_group qdg, *quadg;
	map_node *from, *real_from, *real_gfrom;
	int retries=0, ret;

	from=real_from=real_gfrom=0;
	quadg=&qdg;

	if(!rip_quadg)
		iptoquadg(rip, me.ext_map, quadg, QUADG_GID|QUADG_GNODE);
	else 
		quadg=rip_quadg;
	
	if(!quadg_gids_cmp(*quadg, me.cur_quadg, level))
		return 0;

	/* 
	 * Now, let's check if we are part of the bcast_hdr->g_node of 
	 * bcast_hdr->level. If not let's  drop it! Why the hell this pkt is 
	 * here?
	 */
	if(quadg_gids_cmp(*quadg, me.cur_quadg, level+1)) {
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
		return -1;
	}
	
	/*
	 * `from' has to be absolutely one of our rnodes
	 */
	
	if(!level) {
		iptomap((u_int)me.int_map, rip, me.cur_quadg.ipstart[1], &real_from);
		from = node_from_pos(tracer[hops-1].node, me.int_map);
	} else {
		real_gfrom = &quadg->gnode[_EL(level)]->g;
		from = node_from_pos(quadg->gid[0], me.int_map);
	}
	
	/* Look for the `from' node in the int_map. */
	if((real_from && real_from == from) || from) {
		/* Is `from' in our rnodes? */
		for(retries=0; 
			(ret=rnode_find(me.cur_node, from)) == -1 && !retries; 
				retries++)
			radar_wait_new_scan();
		if(ret != -1)
			return 0;
	}

	/* `from' is a gnode, look in the ext_map */
	if(level) {
		/* Look in ext_map */
		from=(map_node *)gnode_from_pos(tracer[hops-1].node,
				me.ext_map[_EL(level)]);
		if(!from || (real_gfrom && real_gfrom != from)) {
			debug(DBG_INSANE, "%s:%d", ERROR_POS);
			return -1;
		}

		ret=g_rnode_find(me.cur_quadg.gnode[_EL(level)], (map_gnode *)from);
		if(ret == -1) {
			debug(DBG_INSANE, "%s:%d gnode: %d, level: %d", 
					ERROR_POS, tracer[hops-1].node, level);
			return -1;
		}
	}

	return 0;
}

/* 
 * tracer_add_entry: Append our entry `node' to the tracer pkt `tracer' wich has 
 * `hops'. It returns the modified tracer pkt in a newly mallocated struct and
 * it increments the `*hops'.
 * If `tracer' is null it will return the new tracer_pkt.
 * On errors it returns NULL.
 */
tracer_chunk *
tracer_add_entry(void *void_map, void *void_node, tracer_chunk *tracer, 
		u_int *hops, u_char level)
{
	tracer_chunk *t;
	map_node *from;
	map_rnode *rfrom=0;
	map_node  *map, *node;
	map_gnode **ext_map, *gnode;
	int pos, new_entry_pos, last_entry_node, nhops, gcount;

	map=(map_node *)void_map;
	node=(map_node *)void_node;
	ext_map=(map_gnode **)void_map;
	gnode=(map_gnode *)void_node;

	(*hops)++;
	nhops=*hops;
	new_entry_pos=nhops-1;
	t=xmalloc(sizeof(tracer_chunk) * nhops);
	memset(t, 0, sizeof(tracer_chunk) * nhops);
	
	if(tracer || nhops > 1) {
		/* 
		 * In the tracer_pkt there are already some chunks, we copy 
		 * them in the new pkt.
		 */
		memcpy(t, tracer, sizeof(tracer_chunk) * (nhops-1));


		/* 
		 * We add, in the new entry, the rtt there is from me to the 
		 * node of the the last entry of the old tracer pkt.
		 */

		last_entry_node=tracer[nhops-2].node;
		if(!level) {
			from=node_from_pos(last_entry_node, map);
			
			/* check if `from' is in our rnodes */
			if((pos=rnode_find(me.cur_node, from)) == -1) {
				debug(DBG_INSANE, "%s:%d lvl: %d last_entry_node: %d",
						ERROR_POS, level, last_entry_node);
				return 0;
			}
			
			rfrom=&me.cur_node->r_node[pos];
			gcount=1;
		} else {
			from=(map_node*)gnode_from_pos(last_entry_node, 
					ext_map[_EL(level)]);
			
			/* check if `from' is in our rnodes */
			if((pos=g_rnode_find(me.cur_quadg.gnode[_EL(level)], 
							(map_gnode *)from) == -1)) {
				debug(DBG_INSANE, "%s:%d lvl: %d last_entry_node: %d",
						ERROR_POS, level, last_entry_node);
				return 0;
			}

			rfrom=&me.cur_quadg.gnode[_EL(level)]->g.r_node[pos];
			gcount = qspn_gnode_count[_EL(level)];
		}
		t[new_entry_pos].rtt=MILLISEC(rfrom->rtt);
	}

	/* Fill the new entry in the tracer_pkt */
	if(!level)
		t[new_entry_pos].node=pos_from_node(node, map);
	else
		t[new_entry_pos].node=pos_from_gnode(gnode, ext_map[_EL(level)]);
	t[new_entry_pos].gcount=gcount;
	
	return t;
}

/* 
 * tracer_add_rtt: Increments the rtt of the `hop'th `tracer' chunk by adding
 * the rtt of the rnode who is in the `rpos' postion in me.cur_node->r_node.
 * It returns the new rtt value on success.
 */
int tracer_add_rtt(int rpos, tracer_chunk *tracer, u_short hop)
{
	tracer[hop].rtt+=MILLISEC(me.cur_node->r_node[rpos].rtt);
	return tracer[hop].rtt;
}

/* 
 * tracer_get_trtt: It stores in `trtt' the total round trip time needed to
 * reach the `tracer[0].node' from the me.cur_node.
 * me.cur_node->r_node[`from_rnode_pos'] is the rnode who forwarded us the pkt. 
 * If it succeeds 0 is returned.
 */
int tracer_get_trtt(int from_rnode_pos, tracer_hdr *trcr_hdr,
		tracer_chunk *tracer, struct timeval *trtt)
{
	int hops, i;
	u_int trtt_ms=0;
	
	memset(trtt, 0, sizeof(struct timeval));	

	hops = trcr_hdr->hops;
	if(!hops)
		return -1;
	
	/* Add the rtt of me -> from */
	trtt_ms+=MILLISEC(me.cur_node->r_node[from_rnode_pos].trtt);

	for(i=hops-1; i > 0; i--)
		trtt_ms+=tracer[i].rtt;

	MILLISEC_TO_TV(trtt_ms, (*trtt));

	return 0;
}

/*
 * tracer_get_tgcount: it returns the sum of all the gcount values present in
 * each entry of the `tracer' pkt. It doesn't sum all the entries which are in
 * positions > `first_hop'.
 */
u_int tracer_get_tgcount(tracer_hdr *trcr_hdr, tracer_chunk *tracer,
		int first_hop)
{
	u_int i, hops, tgcount=0;	
	
	hops = trcr_hdr->hops;
	if(!hops)
		return 0;

	for(i=first_hop; i>=0; i--)
		tgcount+=tracer[i].gcount;

	return tgcount;
}

/* 
 * tracer_build_bentry: It builds the bnode_block to be added in the bnode's 
 * entry in the tracer pkt. It stores in `bnodechunk' the pointer to the 
 * first bnode_chunk and returns a pointer to the bnode_hdr.
 * `bnode_hdr' and `bnode_chunk' are on the same block of alloced memory.
 * The number of bnode_chunks is stored in `bnode_links'.
 * On errors it returns a NULL pointer.
 */
bnode_hdr *tracer_build_bentry(void *void_map, void *void_node, 
		quadro_group *node_quadg, bnode_chunk **bnodechunk,
		u_short *bnode_links, u_char level)
{
	map_node  *int_map, *node;
	map_gnode **ext_map, *gnode;
	map_gnode *gn;
	bnode_hdr *bhdr;
	bnode_chunk *bchunk;
	int i, bm, node_pos;
	size_t bblock_sz;
	u_char lvl;
	char *bblock;
	u_char *bnode_gid;

	int_map=(map_node *)void_map;
	node=(map_node *)void_node;
	ext_map=(map_gnode **)void_map;
	gnode=(map_gnode *)void_node;
	
	if(level == me.cur_quadg.levels-1)
		goto error;

	if(!level)
		node_pos=pos_from_node(node, int_map);
	else
		node_pos=pos_from_gnode(gnode, ext_map[_EL(level)]);
	
	bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], node_pos);
	if(bm==-1)
		goto error;

	/*This will never happen, but we know the universe is fucking bastard*/
	if(!me.bnode_map[level][bm].links)
		goto error;

	bblock_sz = BNODEBLOCK_SZ(level+1, me.bnode_map[level][bm].links);
	bblock=xmalloc(bblock_sz);
	memset(bblock, 0, bblock_sz);

	bhdr=(bnode_hdr *)bblock;
	bhdr->bnode_levels=level+1;

	bnode_gid=(u_char *)(bblock + sizeof(bnode_hdr));
	bchunk=(bnode_chunk *)(bnode_gid + sizeof(u_char)*bhdr->bnode_levels);
	
	for(i=0; i<bhdr->bnode_levels; i++)
		bnode_gid[i] = node_quadg->gid[i];
	
	/* Fill the bnode chunks */
	for(i=0; i < me.bnode_map[level][bm].links; i++) {
		gn=(map_gnode *)me.bnode_map[level][bm].r_node[i].r_node;
		lvl=extmap_find_level(me.ext_map, gn, me.cur_quadg.levels);
	
		if(lvl != level+1)
			continue;

		bchunk[i].gnode=pos_from_gnode(gn, me.ext_map[_EL(lvl)]);
		bchunk[i].level=lvl;
		memcpy(&bchunk[i].rtt, &me.bnode_map[level][bm].r_node[i].rtt, 
				sizeof(struct timeval));
		
		bhdr->links++;
		
		debug(DBG_INSANE, "tracer_build_bentry: lvl %d bchunk[%d].gnode:"
				" %d", level, i, bchunk[i].gnode);
	}

	if(!bhdr->links) {
		xfree(bblock);
		goto error;
	}	
	
	/* Reduce the size of the bblock to its effective size. Initially we 
	 * alloced it considering all the `me.bnode_map[level][bm].links' 
	 * links, but if bhdr->links is lesser than 
	 * me.bnode_map[level][bm].links that means they are not all added in
	 * the chunks.
	 */
	if(bhdr->links < me.bnode_map[level][bm].links) {
		bblock_sz = BNODEBLOCK_SZ(bhdr->bnode_levels, bhdr->links);
		bblock = xrealloc(bblock, bblock_sz);
		bhdr=(bnode_hdr *)bblock;
		bchunk=(bnode_chunk *)(bblock + BNODE_HDR_SZ(bhdr->bnode_levels));
	}

	*bnode_links=bhdr->links;
	*bnodechunk=bchunk;
	return bhdr;
error:
	*bnode_links=0;
	*bnodechunk=0;
	return 0;
}

/* 
 * tracer_pack_pkt: do ya need explanation? pretty simple: pack the tracer packet
 */
char *tracer_pack_pkt(brdcast_hdr *bcast_hdr, tracer_hdr *trcr_hdr, tracer_chunk *tracer, 
		      char *bblocks, size_t bblocks_sz)
{
	bnode_hdr *bhdr;
	bnode_chunk *bchunk;
	size_t pkt_sz;
	char *msg, *buf;
	int i;

	pkt_sz=BRDCAST_SZ(TRACERPKT_SZ(trcr_hdr->hops) + bblocks_sz);
	
	buf=msg=xmalloc(pkt_sz);
	memset(msg, 0, pkt_sz);

	/* add broadcast header */
	memcpy(buf, bcast_hdr, sizeof(brdcast_hdr));
	ints_host_to_network(buf, brdcast_hdr_iinfo);
	buf+=sizeof(brdcast_hdr);
	
	/* add the tracer header */
	memcpy(buf, trcr_hdr, sizeof(tracer_hdr));
	ints_host_to_network(buf, tracer_hdr_iinfo);
	buf+=sizeof(tracer_hdr);

	/* add the tracer chunks and convert them to network order */
	for(i=0; i<trcr_hdr->hops; i++) {
		memcpy(buf, &tracer[i], sizeof(tracer_chunk));
		ints_host_to_network(buf, tracer_chunk_iinfo);
		
		buf+=sizeof(tracer_chunk);
	}

	/* add the bnode blocks */
	if(bblocks_sz && bblocks) {
		/* copy the whole block */
		memcpy(buf, bblocks, bblocks_sz);
	
		/* and convert it to network order */
		bhdr=(bnode_hdr *)buf;
		bchunk=(bnode_chunk *)((char *)buf+sizeof(bnode_hdr)+
				sizeof(u_char)*bhdr->bnode_levels);
		
		for(i=0; i<bhdr->links; i++)
			ints_host_to_network(&bchunk[i], bnode_chunk_iinfo);

		ints_host_to_network(bhdr, bnode_hdr_iinfo);

		buf+=bblocks_sz;
	}

	return msg;
}

/* 
 * tracer_unpack_pkt: Given a packet `rpkt' it scomposes the rpkt.msg in 
 * `new_bcast_hdr', `new_tracer_hdr', `new_tracer', 'new_bhdr', and 
 * `new_block_sz'.
 * If the `new_rip_quadg' pointer is not null, the quadro_group of the 
 * `rpk.from' ip is stored in it.
 * It returns 0 if the packet is valid, otherwise -1 is returned.
 * Note that rpkt.msg will be modified during the unpacking.
 */
int tracer_unpack_pkt(PACKET rpkt, brdcast_hdr **new_bcast_hdr, 
		      tracer_hdr **new_tracer_hdr, tracer_chunk **new_tracer, 
		      bnode_hdr **new_bhdr, size_t *new_bblock_sz,
		      quadro_group *new_rip_quadg, int *real_from_rpos)
{
	brdcast_hdr *bcast_hdr;
	tracer_hdr  *trcr_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	quadro_group rip_quadg;
	size_t bblock_sz=0, tracer_sz=0;
	int level, i;

	bcast_hdr=(brdcast_hdr *)rpkt.msg;
	ints_network_to_host(bcast_hdr, brdcast_hdr_iinfo);
	
	trcr_hdr=(tracer_hdr *)(rpkt.msg+sizeof(brdcast_hdr));
	ints_network_to_host(trcr_hdr, tracer_hdr_iinfo);
	
	tracer=(tracer_chunk *)(rpkt.msg+sizeof(brdcast_hdr)+sizeof(tracer_hdr));

	*new_bcast_hdr=0;
	*new_tracer_hdr=0;
	*new_tracer=0;
	*new_bhdr=0;
	*new_bblock_sz=0;
	*real_from_rpos=0;

	tracer_sz=BRDCAST_SZ(TRACERPKT_SZ(trcr_hdr->hops));
	if(tracer_sz > rpkt.hdr.sz || !trcr_hdr->hops || 
			trcr_hdr->hops > MAXGROUPNODE) {
		debug(DBG_INSANE, "%s:%d messed tracer pkt: %d, %d, %d", 
				ERROR_POS, tracer_sz, rpkt.hdr.sz, 
				trcr_hdr->hops);
		return -1;
	}
	
	/* Convert the tracer chunks to host order */
	for(i=0; i<trcr_hdr->hops; i++)
		ints_network_to_host(&tracer[i], tracer_chunk_iinfo);
	
	if(rpkt.hdr.sz > tracer_sz) {
		/* There is also a bnode block in the tracer pkt */

		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=(bnode_hdr *)(rpkt.msg+tracer_sz);
		if(!trcr_hdr->bblocks || !(bcast_hdr->flags & BCAST_TRACER_BBLOCK)){
			debug(DBG_INSANE, "%s:%d links: %d flags: %d", ERROR_POS, 
					trcr_hdr->bblocks, bcast_hdr->flags);
			return -1;
		}
	}

	if((level=bcast_hdr->level) > 0)
		level--;
	if(!(rpkt.hdr.flags & BCAST_PKT) || !(bcast_hdr->flags & BCAST_TRACER_PKT) || 
			level > GET_LEVELS(rpkt.from.family)) {
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
			return -1;
	}

	/* Convert the ip in quadro_group */
	iptoquadg(rpkt.from, me.ext_map, &rip_quadg, QUADG_GID|QUADG_GNODE);
	memcpy(new_rip_quadg, &rip_quadg, sizeof(quadro_group));

	if(tracer_verify_pkt(tracer, trcr_hdr->hops, rpkt.from, &rip_quadg, level))
		return -1;
	
	*real_from_rpos=ip_to_rfrom(rpkt.from, &rip_quadg, 0, 0);
	if(*real_from_rpos < 0) {
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
		return -1;
	}

	
	*new_bcast_hdr=bcast_hdr;
	*new_tracer_hdr=trcr_hdr;
	*new_tracer=tracer;
	*new_bhdr=bhdr;
	*new_bblock_sz=bblock_sz;
	return 0;
}

/* 
 * tracer_split_bblock: It searches from bnode_block_start to 
 * bnode_block_start+bblock_sz for bnode blocks.
 * It puts the address of the found bblock_hdr in the `bbl_hdr' (bnode block list)
 * and the address pointing to the start of the bnode_chunk in the `bbl'. The 
 * total size of all the valid bblocks considered is stored in `*bblock_found_sz'.
 * It then returns the number of bblock found. 
 * 
 * During the splitting the bblock is modified 'cause it is converted in host
 * order.
 * Remember to xfree bbl_hdr and bbl after using tracer_split_bblock too. 
 * On error zero is returned.
 */
u_short tracer_split_bblock(void *bnode_block_start, size_t bblock_sz, bnode_hdr ***bbl_hdr, 
		        bnode_chunk ****bbl, size_t *bblock_found_sz)
{
	bnode_hdr 	*bblock_hdr;
	bnode_chunk 	*bblock;
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	u_char 		*bnode_gid;
	size_t 		bsz=0;
	int e,p,x=0;
		
	*bblock_found_sz=0;
	if(!bblock_sz)
		return 0;

	for(e=0, x=0; e < bblock_sz; ) {
		bblock_hdr=(void *)((char *)bnode_block_start + e);
		ints_network_to_host(bblock_hdr, bnode_hdr_iinfo);
		
		bnode_gid =(char *)bblock_hdr+sizeof(bnode_hdr);
		bblock=(bnode_chunk *)((char *)bnode_gid +
				(bblock_hdr->bnode_levels*sizeof(u_char)));

		if(bblock_hdr->links <= 0) {
			e+=BNODEBLOCK_SZ(bblock_hdr->bnode_levels, 0);
			continue;
		}
		if(bblock_hdr->links >= MAXGROUPNODE)
			goto skip;

		bsz=BNODEBLOCK_SZ(bblock_hdr->bnode_levels, bblock_hdr->links);
		
		/*Are we going far away the end of the buffer?*/
		if(bblock_sz-e < bsz)
			break;
		
		bblist_hdr=xrealloc(bblist_hdr, sizeof(bnode_hdr *) * (x+1));
		bblist=xrealloc(bblist, sizeof(bnode_chunk *) * (x+1));

		bblist_hdr[x]=bblock_hdr;
		bblist[x]=xmalloc(sizeof(bnode_chunk *) * bblock_hdr->links);
		for(p=0; p<bblock_hdr->links; p++) {
			bblist[x][p]=&bblock[p];
			ints_network_to_host(&bblock[p], bnode_chunk_iinfo); 
		}
		
		(*bblock_found_sz)+=bsz;
		x++;
skip:
		e+=bsz;
	}
	
	*bbl_hdr=bblist_hdr;
	*bbl=bblist;
	return x;
}


/* 
 * tracer_store_pkt: This is the main function used to keep the int/ext_map's
 * karma in peace.
 * It updates the `map' with the given `tracer' pkt. The bnode blocks (if any) 
 * are unpacked and used to update the data of the bordering gnodes.
 * In `*bblocks_found' it stores the number of bblocks considered and stores in
 * `bblocks_found_block' these bblocks. The `bblocks_found_block' remains in 
 * network order because it will be appended in the new tracer_pkt, after our 
 * bblock entry (if any). 
 * Remember to xfree(bblocks_found_block);
 */
int tracer_store_pkt(inet_prefix rip, quadro_group *rip_quadg, u_char level, 
		     tracer_hdr *trcr_hdr,    tracer_chunk *tracer,
		     void *bnode_block_start, size_t bblock_sz, 
		     u_short *bblocks_found,  char **bblocks_found_block, 
		     size_t *bblock_found_sz)
{
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	map_node *from, *node, *root_node;
	void *void_node;
	map_gnode *gfrom, *gnode=0;
	map_rnode rn, rnn;
			
	int i, e, o, x, f, p, diff, bm, from_rnode_pos, skip_rfrom;
	int first_gcount_hop, gfrom_rnode_pos, from_tpos;
	u_int hops, trtt_ms=0, tgcount=0;
	u_short bb;
	size_t found_block_sz, bsz;
	char *found_block;
	u_char *bnode_gid, bnode, blevel;


	hops = trcr_hdr->hops;
	/* Nothing to store */
	if(hops <= 0)
		return 0;
	else if(hops <= 1 && !bblock_sz)
		return 0;
	
	if(!level) {
	 	from   	       = node_from_pos(tracer[hops-1].node, me.int_map);
		root_node      = me.cur_node;
	} else {
		gfrom	       = gnode_from_pos(tracer[hops-1].node, me.ext_map[_EL(level)]);
		from	       = &gfrom->g;
		root_node      = &me.cur_quadg.gnode[_EL(level)]->g;
	}
	from_rnode_pos = rnode_find(root_node, from);
	from_tpos      = hops-1;

	/* It's alive, keep it young */
	from->flags&=~QSPN_OLD;

	if(bblock_sz && level != me.cur_quadg.levels-1) {

		/* Well, well, we have to take care of bnode blocks, split the
		 * bblock. */
		bb=tracer_split_bblock(bnode_block_start, bblock_sz, &bblist_hdr,
				&bblist, &found_block_sz);
		*bblocks_found = bb;
		if(!bb) {
			/* The bblock was malformed -_- */
			debug(DBG_NORMAL, "%s:%d: malformed bnode block", ERROR_POS);
			*bblock_found_sz = 0;
			*bblocks_found_block = 0;
		} else {
			
			/*
			 * Store the received bnode blocks 
			 */
			
			if(bb != trcr_hdr->bblocks) 
				debug(DBG_NOISE, "%s:%d: Skipping some bblocks of the "
						"tracer_pkt", ERROR_POS);
			x=0;
			*bblock_found_sz=found_block_sz;
			*bblocks_found_block=found_block=xmalloc(found_block_sz);
			for(i=0; i<bb; i++) {
				
				bnode_gid=(char *)bblist_hdr[i] + sizeof(bnode_hdr);

				/* We update only the bmaps which are at
				 * levels where our gnodes are in common with
				 * those of the bnode, which sent us this
				 * bblock */
				for(o=level, f=0; o >= 0; o--)
					if(bnode_gid[o] != me.cur_quadg.gid[o]) {
						f=1;
						break;
					}
				if(!f) { 
					/*
					 * bnode_gid is equal to me.cur_quadg.gid, so this 
					 * bnode block was sent by ourself, skip it. 
					 */

					debug(DBG_NORMAL, ERROR_MSG "skipping the %d bnode,"
							"it was built by us!", ERROR_POS, i);
					xfree(bblist[i]);
					continue;
				}
					

				for(blevel=o; blevel < bblist_hdr[i]->bnode_levels; blevel++) {
					bnode=bnode_gid[blevel];

					if(!blevel) {
						node=node_from_pos(bnode, me.int_map);
						node->flags|=MAP_BNODE;
						node->flags&=~QSPN_OLD;
						void_node=(void *)node;
					} else {
						gnode=gnode_from_pos(bnode, me.ext_map[_EL(blevel)]);
						gnode->g.flags|=MAP_BNODE;
						gnode->g.flags&=~QSPN_OLD;
						void_node=(void *)&gnode->g;
					}

					/* Let's check if we have this bnode in the bmap, if not let's 
					 * add it */
					bm=map_find_bnode(me.bnode_map[blevel], me.bmap_nodes[blevel], 
							bnode);
					if(bm==-1)
						bm=map_add_bnode(&me.bnode_map[blevel], 
								&me.bmap_nodes[blevel],
								bnode,  bblist_hdr[i]->links);

					/* This bnode has the BMAP_UPDATE
					 * flag set, thus this is the first
					 * time we update him during this new
					 * qspn_round and for this reason
					 * delete all its rnodes */
					if(me.bnode_map[blevel][bm].flags & BMAP_UPDATE) {
						rnode_destroy(&me.bnode_map[blevel][bm]);
						me.bnode_map[blevel][bm].flags&=~BMAP_UPDATE;
					}
					
					/* Store the rnodes of the bnode */
					for(e=0; e < bblist_hdr[i]->links; e++) {
						memset(&rn, 0, sizeof(map_rnode));
						debug(DBG_INSANE, "Bnode %d new link %d: gid %d lvl %d", 
								bnode, e, bblist[i][e]->gnode,
								bblist[i][e]->level);

						gnode=gnode_from_pos(bblist[i][e]->gnode, 
								me.ext_map[_EL(bblist[i][e]->level)]);
						gnode->g.flags&=~QSPN_OLD;

						rn.r_node=(int *)gnode;
						memcpy(&rn.rtt, &bblist[i][e]->rtt, 
								sizeof(struct timeval));

						if((p=rnode_find(&me.bnode_map[blevel][bm], gnode)) > 0) {
							/* Overwrite the current rnode */
							map_rnode_insert(&me.bnode_map[blevel][bm],p,&rn);
						} else
							/* Add a new rnode */
							rnode_add(&me.bnode_map[blevel][bm], &rn);
					}
				}

				/* Copy the found bblock in `bblocks_found_block' and converts
				 * it in network order */
				bsz=BNODEBLOCK_SZ(bblist_hdr[i]->bnode_levels, bblist_hdr[i]->links);
				memcpy(found_block+x, bblist_hdr[i], bsz);
				ints_host_to_network(found_block+x, bnode_hdr_iinfo);
				ints_host_to_network(found_block+x+sizeof(bnode_hdr), bnode_chunk_iinfo);
				x+=bsz;
			
				xfree(bblist[i]);
			}

			xfree(bblist_hdr);
			xfree(bblist);
		}
	}
	
	
	/* 
	 * * Store the qspn routes to reach all the nodes of the tracer pkt *
	 */

	
	skip_rfrom=0;
	node=root_node;
	first_gcount_hop=hops-1;
	if(!level) {
		/* We skip the node at hops-1 which it is the `from' node. The radar() 
		 * takes care of him. */
		skip_rfrom = 1;
	} else if(from == root_node) {
		/* If tracer[hops-1].node is our gnode then we can skip it */
		skip_rfrom = 1;
		first_gcount_hop=hops-2;
		from_rnode_pos=ip_to_rfrom(rip, rip_quadg, 0, 0);

		if(hops > 1) {
			map_rnode rnn;
			
			/* 
			 * hops-2 is an rnode of hops-1, which is our gnode,
			 * so we update the `gfrom' and `from' vars and let
			 * them point to hops-2.
			 */
			gfrom=gnode_from_pos(tracer[hops-2].node,
					me.ext_map[_EL(level)]);
			from = &gfrom->g;
			from->flags|=MAP_GNODE | MAP_RNODE;
			from_tpos = hops-2;

			gfrom_rnode_pos=rnode_find(root_node, gfrom);
			if(gfrom_rnode_pos == -1) {
				gfrom_rnode_pos=root_node->links;
				
				/*
				 * Add an rnode in the root_node which point to
				 * `gfrom', because it is our new (g)rnode.
				 */
				memset(&rnn, '\0', sizeof(map_rnode));
				rnn.r_node=(int *)gfrom;
				rnode_add(root_node, &rnn);
			}
			MILLISEC_TO_TV(tracer[hops-2].rtt, root_node->r_node[gfrom_rnode_pos].rtt);
		}

		/* we are using the real from, so the root node is the one
		 * at level 0 */
		node=me.cur_node;
	} else if(me.cur_node->flags & MAP_BNODE) {
		/* If we are a bnode which borders on the `from' [g]node, then we
		 * can skip it. */
		i=map_find_bnode_rnode(me.bnode_map[level-1], me.bmap_nodes[level-1], from);
		if(i != -1)
			skip_rfrom = 1;
	}

	/* Let's see if we have to rehook */
	new_rehook((map_gnode *)from, tracer[from_tpos].node, level,
			tracer[from_tpos].gcount);

	/* Get the total gnode count and update `qspn_gnode_count' */
	tgcount=tracer_get_tgcount(trcr_hdr, tracer, first_gcount_hop);
	qspn_inc_gcount(qspn_gnode_count, level, tgcount);
	
	/* We add in the total rtt the first rtt which is me -> from */
	trtt_ms=MILLISEC(node->r_node[from_rnode_pos].trtt);

	/* If we are skipping the rfrom, remember to sum its rtt */
	if(skip_rfrom)
		trtt_ms+=tracer[hops-1].rtt;

	for(i=(hops-skip_rfrom)-1; i >= 0; i--) {
		if(i)
			trtt_ms+=tracer[i].rtt;

		if(!level) {
			node=node_from_pos(tracer[i].node, me.int_map);
			if(node == me.cur_node) {
				debug(DBG_INSANE, "Ehi! There's a hop in the "
						"tracer pkt which points to me");
				new_rehook((map_gnode *)node, tracer[i].node, 
						level, 0);
				break;
			}
		} else {
			gnode=gnode_from_pos(tracer[i].node, me.ext_map[_EL(level)]);
			node=&gnode->g;

			if(tracer[i].gcount == NODES_PER_LEVEL(level))
				/* The gnode is full */
				gnode->g.flags|=GMAP_FULL;
			
			if(gnode == me.cur_quadg.gnode[_EL(level)] && 
					gnode->g.flags & MAP_BNODE) {
				debug(DBG_INSANE, "There's a hop in the "
						"tracer pkt which points to me");
				new_rehook(gnode, tracer[i].node, level,
						tracer[i].gcount);
				break;
			}
		}
		node->flags&=~QSPN_OLD;
			
		if(node->flags & MAP_VOID) { 
			/* Ehi, we hadn't this node in the map. Add it. */
			node->flags&=~MAP_VOID;
			node->flags|=MAP_UPDATE;
			if(level)
				gnode->flags&=~GMAP_VOID;
	
			gnode_inc_seeds(&me.cur_quadg, level);
			debug(DBG_INSANE, "TRCR_STORE: node %d added", tracer[i].node);
		}
		
		/* update the rtt of the node */
		for(e=0,f=0; e < node->links; e++) {
			if(node->r_node[e].r_node == (int *)from) {
				diff=abs(MILLISEC(node->r_node[e].trtt) - trtt_ms);
				if(diff >= RTT_DELTA) {
					MILLISEC_TO_TV(trtt_ms, node->r_node[e].trtt);
					node->flags|=MAP_UPDATE;
				}
				f=1;
				break;
			}
		}
		if(!f) { 
			/*If the `node' doesn't have `from' in his r_nodes... let's add it*/
			memset(&rnn, '\0', sizeof(map_rnode));

			rnn.r_node=(int *)from;
			MILLISEC_TO_TV(trtt_ms, rnn.trtt);
			
			rnode_add(node, &rnn);
			node->flags|=MAP_UPDATE;
		}

		/* ok, now the kernel needs a refresh of the routing table */
		if(node->flags & MAP_UPDATE) {
			rnode_trtt_order(node);
			
			if(node->links > MAXROUTES) { 
				/* 
				 * If we have too many routes we purge the worst
				 * ones.
				 */
				for(x=MAXROUTES; x < node->links; x++)
					rnode_del(node, x);
			}
			
			debug(DBG_INSANE, "TRCR_STORE: krnl_update node %d", tracer[i].node);
			rt_update_node(0, node, 0, 0, 0, level);
			node->flags&=~MAP_UPDATE;
		}
	}
	return 0;
}


/* 
 * tracer_pkt_build: It builds a tracer_pkt and stores it in `pkt'.
 * If `trcr_hdr' or `tracer' are null, it will build a brand new tracer_pkt, 
 * otherwise it will append in the `tracer' the new entry. Tracer_pkt_build 
 * will append also the old bblock: `old_bchunks' is the number of bblocks, 
 * `old_bblock' is the block of the old bblock and it is `old_bblock_sz'. 
 * If `old_bchunks' is 0 or `old_bblock' and `old_bblock_sz' are null they 
 * are ignored.
 * The `pkt.hdr.op' is set to `rq', `pkt.hdr.id' to `rq_id' and the 
 * `bcast_hdr.sub_id' to `bcast_sub_id'.
 * The packet shall be sent with flood_pkt_send.
 * It returns -1 on errors. 
 */
int tracer_pkt_build(u_char rq,   	     int rq_id, 	     int bcast_sub_id,
		     int gnode_id,	     u_char gnode_level,
		     brdcast_hdr *bcast_hdr, tracer_hdr *trcr_hdr,   tracer_chunk *tracer,  
		     u_short old_bchunks,    char *old_bblock,       size_t old_bblock_sz,  
		     PACKET *pkt)
{
	brdcast_hdr bh;
	tracer_hdr th;
	
	tracer_chunk *new_tracer=0;
	bnode_hdr    *new_bhdr=0;
	bnode_chunk  *new_bchunk=0;
	map_node *root_node, *upper_root_node=0;
	void *void_map, *void_node, *p;
	size_t new_bblock_sz=0, total_bblock_sz=0;
	u_int hops=0;

	if(!trcr_hdr || !tracer || !bcast_hdr) {
		/* Brand new tracer packet */
		bcast_hdr=&bh;
		memset(bcast_hdr, 0, sizeof(brdcast_hdr));
		
		bcast_hdr->gttl=MAXGROUPNODE-1;
		bcast_hdr->level=gnode_level+1;
		bcast_hdr->g_node=gnode_id; 
		
		trcr_hdr=&th;
		memset(trcr_hdr, 0, sizeof(tracer_hdr));
	} 
	
	hops=trcr_hdr->hops;

	memset(pkt, 0, sizeof(PACKET));
	pkt->hdr.op=rq;
	pkt->hdr.id=rq_id;
	pkt->hdr.flags|=BCAST_PKT;
	bcast_hdr->flags|=BCAST_TRACER_PKT;
	
	if(!gnode_level) {
		void_map=(void *)me.int_map;
		root_node=me.cur_node;
		void_node=(void *)root_node;
	} else {
		void_map=(void *)me.ext_map;
		root_node=&me.cur_quadg.gnode[_EL(gnode_level)]->g;
		void_node=(void *)root_node;
	}
	
	if(gnode_level < me.cur_quadg.levels)
	upper_root_node=&me.cur_quadg.gnode[_EL(gnode_level+1)]->g;


	/* Time to append our entry in the tracer_pkt */
	new_tracer=tracer_add_entry(void_map, void_node, tracer, &hops, 
			gnode_level); 
	if(!new_tracer) {
		debug(DBG_NOISE, "tracer_pkt_build: Cannot add the new"
				" entry in the tracer_pkt");
		return -1;
	}

	/* If we are a bnode we have to append the bnode_block too. */
	if(me.cur_node->flags & MAP_BNODE &&
			gnode_level < me.cur_quadg.levels-1 &&
			upper_root_node->flags & MAP_BNODE) {

		new_bhdr=tracer_build_bentry(void_map, void_node,&me.cur_quadg,
				&new_bchunk, &trcr_hdr->bblocks, gnode_level);
		if(new_bhdr) {
			new_bblock_sz=BNODEBLOCK_SZ(new_bhdr->bnode_levels, 
					trcr_hdr->bblocks);
			bcast_hdr->flags|=BCAST_TRACER_BBLOCK;
		}
	}

	/*
	 * If in the old tracer_pkt is present a bblock, we append it after the 
	 * new entry.
	 */
	if(old_bchunks && old_bblock && old_bblock_sz) {
		total_bblock_sz = new_bblock_sz + old_bblock_sz;
		
		new_bhdr=xrealloc(new_bhdr, total_bblock_sz);
		new_bchunk=(bnode_chunk *)((char *)new_bhdr + 
				BNODE_HDR_SZ(new_bhdr->bnode_levels));
	
		p=(char *)new_bchunk + new_bblock_sz;
		memcpy(p, old_bblock, old_bblock_sz);
		
		trcr_hdr->bblocks += old_bchunks;
		bcast_hdr->flags|=BCAST_TRACER_BBLOCK;
	}

	/* 
	 * Here we are really building the pkt, packing all the stuff into a
	 * single bullet.
	 */
	trcr_hdr->hops=hops;
	bcast_hdr->sub_id=bcast_sub_id;
	bcast_hdr->sz=TRACERPKT_SZ(hops)+new_bblock_sz;
	pkt->hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	
	pkt->msg=tracer_pack_pkt(bcast_hdr, trcr_hdr, new_tracer,
				(char *)new_bhdr, new_bblock_sz);
	
	/* Yea, finished */
	if(new_tracer)
		xfree(new_tracer);	
	if(new_bhdr)
		xfree(new_bhdr);
	return 0;
}

/* 
 * flood_pkt_send: This functions is used to propagate packets, in a broadcast
 * manner, in a entire gnode of a specified level.
 * It sends the `pkt' to all the nodes excluding the excluded nodes. It knows 
 * if a node is excluded by calling the `is_node_excluded' function. The 
 * second argument to this function is the node who sent the pkt and it must be
 * always excluded. The third argument is the position of the node being processed
 * in the r_node array of me.cur_node. The other arguments are described in
 * tracer.h.
 * If `is_node_excluded' returns a non 0 value, the node is considered as excluded.
 * The `from_rpos' argument is the node who sent the `pkt'.
 * It returns the number of pkts sent or -1 on errors. Note that the total pkt sent
 * should be == me.cur_node->links-the_excluded_nodes.
 * Note that `level', `sub_id', and `from_rpos' are vars used only by
 * is_node_excluded() (see tracer.h).
 */
int flood_pkt_send(int(*is_node_excluded)(TRACER_PKT_EXCLUDE_VARS), u_char level,
		int sub_id, int from_rpos, PACKET pkt)
{
	inet_prefix to;
	ext_rnode *e_rnode;
	map_node *dst_node, *node;
	ssize_t err;
	const char *ntop;
	int i, e=0;

	/*Forward the pkt to all our r_nodes (excluding the excluded;)*/
	for(i=0; i < me.cur_node->links; i++) {
		node=(map_node *)me.cur_node->r_node[i].r_node;
		if(node->flags & MAP_ERNODE) {
			e_rnode=(ext_rnode *)node;
			dst_node=(map_node *)e_rnode->quadg.gnode[_EL(level-1)];
		} else {
			e_rnode=0;
			dst_node=node;
		}

		if(!dst_node)
			continue;
		if(is_node_excluded(e_rnode, dst_node, from_rpos, i, level, sub_id))
			continue;

		/* We need the ip of the rnode ;^ */
		rnodetoip((u_int)me.int_map, (u_int)node, 
				me.cur_quadg.ipstart[1], &to);
		
		debug(DBG_INSANE, "flood_pkt_send(0x%x): %s to %s lvl %d", 
				pkt.hdr.id, rq_to_str(pkt.hdr.op),
				inet_to_str(to), level-1);
				
		pkt_addto(&pkt, &to);

		/*Let's send the pkt*/
		pkt_add_dev(&pkt, rnl_get_dev(rlist, node), 1);
		err=send_rq(&pkt, 0, pkt.hdr.op, pkt.hdr.id, 0, 0, 0);
		if(err==-1) {
			ntop=inet_to_str(pkt.to);
			error("flood_pkt_send(): Cannot send the %s request"
					" with id: %d to %s.", rq_to_str(pkt.hdr.op),
					pkt.hdr.id, ntop);
		} else
			e++;
	}
		
	pkt_free(&pkt, 1);
	return e;
}

/* * * 	Exclude functions * * *
 * These exclude function are used in conjunction with flood_pkt_send. 
 * They return 1 if the node has to be excluded, otherwise 0.
 */

/*
 * exclude_glevel: Exclude `node' if it doesn't belong to the gid (`excl_gid') of 
 * the level (`excl_level') specified.
 */
int exclude_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	/* If `node' is null we can exclude it, because it isn't a gnode
	 * of ours levels */
	if(!node)
		return 1;
	
	/* Ehi, if the node isn't even an external rnode, we don't exclude it. */
	if(!(node->flags & MAP_ERNODE))
		return 0;

	/* Reach the sky */
	if(excl_level == me.cur_quadg.levels)
		return 0;
	
	return quadg_gids_cmp(e_rnode->quadg, me.cur_quadg, excl_level);
}

/* Exclude the `from' node */
int exclude_from(TRACER_PKT_EXCLUDE_VARS)
{
	if(pos == from_rpos)
		return 1;
	return 0;
}

/* Exclude all the nodes, except the from node */
int exclude_all_but_notfrom(TRACER_PKT_EXCLUDE_VARS)
{
	if(!exclude_from(TRACER_PKT_EXCLUDE_VARS_NAME))
		return 1;
	return 0;
}

int exclude_from_and_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	if(exclude_glevel(TRACER_PKT_EXCLUDE_VARS_NAME) || 
			exclude_from(TRACER_PKT_EXCLUDE_VARS_NAME))
		return 1;
	return 0;
}


/* 
 * tracer_pkt_recv: It receive a TRACER_PKT or a TRACER_PKT_CONNECT, analyzes 
 * the received pkt, adds the new entry in it and forward the pkt to all 
 * the r_nodes.
 */
int tracer_pkt_recv(PACKET rpkt)
{
	PACKET pkt;
	brdcast_hdr  *bcast_hdr;
	tracer_hdr   *trcr_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	map_node *from, *tracer_starter, *root_node;
	map_gnode *gfrom;
	quadro_group rip_quadg;
	
	int(*exclude_function)(TRACER_PKT_EXCLUDE_VARS);
	int ret_err, gid, real_from_rpos;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	u_char level, orig_lvl;
	const char *ntop=0;
	char *old_bblock=0;
	void *void_map;

	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &trcr_hdr, &tracer, 
			&bhdr, &bblock_sz, &rip_quadg, &real_from_rpos);
	if(ret_err) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "tracer_pkt_recv(): The %s sent an invalid "
				 "tracer_pkt here.", ntop);
		return -1;
	}

	hops=trcr_hdr->hops;
	gid=bcast_hdr->g_node;
	level=orig_lvl=bcast_hdr->level;
	if(!level || level == 1) {
		level=0;
		root_node      = me.cur_node;
		from	       = node_from_pos(tracer[hops-1].node, me.int_map);
		tracer_starter = node_from_pos(tracer[0].node, me.int_map);
		void_map=me.int_map;
	} else {
		level--;
		root_node      = &me.cur_quadg.gnode[_EL(level)]->g;
		gfrom	       = gnode_from_pos(tracer[hops-1].node, me.ext_map[_EL(level)]);
		from	       = &gfrom->g;
		tracer_starter = (map_node *)gnode_from_pos(tracer[0].node, me.ext_map[_EL(level)]);
		void_map       = me.ext_map;
	}

	if(server_opt.dbg_lvl) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "Tracer_pkt(0x%x, lvl %d) received from %s", 
				rpkt.hdr.id, level, ntop);
	}

	/*
	 * This is the check for the broadcast id. If it is <= tracer_starter->brdcast
	 * the pkt is an old broadcast that still dance around.
	 */
	if(rpkt.hdr.id <= tracer_starter->brdcast) {
		debug(DBG_NOISE, "tracer_pkt_recv(): Received from %s an old "
				"tracer_pkt broadcast: 0x%x, cur: 0x%x", ntop,
				rpkt.hdr.id, tracer_starter->brdcast);
		return -1;
	} else
		tracer_starter->brdcast=rpkt.hdr.id;


	/* 
	 * Time to update our map
	 */
	if(rpkt.hdr.op == TRACER_PKT) { /*This check is made because tracer_pkt_recv 
					 handles also TRACER_PKT_CONNECT pkts*/
		
		ret_err=tracer_store_pkt(rpkt.from, &rip_quadg, level,
				trcr_hdr, tracer, (void *)bhdr,
				bblock_sz, &old_bchunks, &old_bblock,
				&old_bblock_sz);
		if(ret_err) {
			ntop=inet_to_str(rpkt.from);
			debug(DBG_NORMAL, "tracer_pkt_recv(): Cannot store the"
					" tracer_pkt received from %s", ntop);
		}
	}


	/* 
	 * Drop the pkt if it is bound to the contigual qspn starters and we
	 * aren't a qspn_starter
	 */
	if(bcast_hdr->flags & BCAST_TRACER_STARTERS  && 
			!(root_node->flags & QSPN_STARTER))
		return 0;
	
	/*The forge of the packet.*/
	if((!level || ((me.cur_node->flags & MAP_BNODE) && 
					(root_node->flags & MAP_BNODE))) &&
			from != root_node) {
		tracer_pkt_build(rpkt.hdr.op, rpkt.hdr.id, bcast_hdr->sub_id, /*IDs*/
				gid,         level,			    
				bcast_hdr,   trcr_hdr, tracer, 	      	      /*Received tracer_pkt*/
				old_bchunks, old_bblock, old_bblock_sz,       /*bnode_block*/
				&pkt);					      /*Where the pkt is built*/
	} else {
		/* Increment the rtt of the last gnode chunk */
		ret_err=tracer_add_rtt(real_from_rpos, tracer, hops-1);
		if(ret_err < 0)
			debug(DBG_NOISE, "tracer_add_rtt(0x%x) hop %d failed",
					rpkt.hdr.id, hops-1);
		pkt_copy(&pkt, &rpkt);
		pkt_clear(&pkt);
	}
	

	/*... forward the tracer_pkt to our r_nodes*/
	exclude_function=exclude_from_and_glevel;
	flood_pkt_send(exclude_function, orig_lvl, real_from_rpos,
			real_from_rpos, pkt);

	if(old_bblock)
		xfree(old_bblock);
	return 0;
}

/* 
 * tracer_pkt_start: It sends only a normal tracer_pkt. This is useful after 
 * the hook, to let all the other nodes know we are alive and to give them 
 * the right route.
 */
int tracer_pkt_start(u_char level)
{
	PACKET pkt;
	int root_node_pos;
	
	if(tracer_pkt_start_mutex)
		return 0;
	else
		tracer_pkt_start_mutex=1;

	if(!level || level == 1) {
		level=0;
		root_node_pos=pos_from_node(me.cur_node, me.int_map);
	} else
		root_node_pos=pos_from_gnode(me.cur_quadg.gnode[_EL(level)], 
				me.ext_map[_EL(level)]);

	me.cur_node->brdcast++;
	tracer_pkt_build(TRACER_PKT, me.cur_node->brdcast, root_node_pos,/*IDs*/
			 me.cur_quadg.gid[level+1],	   level,	 /*GnodeID and level*/
			 0,          0,                    0, 		 /*Received tracer_pkt*/
			 0,          0,                    0, 		 /*bnode_block*/
			 &pkt);						 /*Where the pkt is built*/
	/*Diffuse the packet in all the universe!*/
	debug(DBG_INSANE, "Tracer_pkt 0x%x starting.", pkt.hdr.id);
	flood_pkt_send(exclude_from_and_glevel, level+1, -1, -1, pkt);
	tracer_pkt_start_mutex=0;
	return 0;
}
