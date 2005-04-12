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

#include "misc.h"
#include "inet.h"
#include "route.h"
#include "pkts.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "tracer.h"
#include "netsukuku.h"
#include "radar.h"
#include "request.h"
#include "xmalloc.h"
#include "log.h"

/* 
 * tracer_verify_pkt: It checks the validity of `tracer': The last entry
 * in the tracer must be a node present in our r_nodes. If `real_from'
 * is non 0, it checks if it is equal to the last entry in the packet. 
 */
int tracer_verify_pkt(tracer_chunk *tracer, u_int hops, map_node *real_from, u_char level)
{
	map_node *from;

	/*
	 * `from' has to be absolutely one of our rnodes
	 */

	if(!level) {
		from=node_from_pos(tracer[hops-1].node, me.int_map);
		if(from && ( from != real_from ))
			return -1;
	
		/* Is `from' in our rnodes? */
		if(rnode_find(me.cur_node, from) == -1)
			return -1;
	} else {
		from=(map_node *)gnode_from_pos(me.cur_quadg.gid[level], 
				me.ext_map[_EL(level)]);
		if(from && (void *)from != (void *)real_from) 
			return -1;
			
		if(g_rnode_find(me.cur_quadg.gnode[_EL(level)], (map_gnode *)from) == -1)
			return -1;
	}
	return 0;
}

/* 
 * tracer_add_entry: Add our entry `node' to the tracer pkt `tracer' wich has 
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
	int pos, new_entry_pos, last_entry_node, nhops;

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
			if((pos=rnode_find(me.cur_node, from)) == -1)
				return 0;
			
			rfrom=&me.cur_node->r_node[pos];
		} else {
			from=(map_node*)gnode_from_pos(last_entry_node, 
					ext_map[_EL(level)]);
			
			/* check if `from' is in our rnodes */
			if((pos=g_rnode_find(me.cur_quadg.gnode[_EL(level)], 
							(map_gnode *)from) == -1))
				return 0;

			rfrom=&me.cur_quadg.gnode[_EL(level)]->g.r_node[pos];
		}
		memcpy(&t[new_entry_pos].rtt, &rfrom->rtt, sizeof(struct timeval));
	}

	if(!level)
		t[new_entry_pos].node=pos_from_node(node, map);
	else
		t[new_entry_pos].node=pos_from_gnode(gnode, ext_map[_EL(level)]);
	
	return t;
}

/* 
 * tracer_build_bentry: It builds the bnode_block to be added in the bnode's 
 * entry in the tracer pkt. It stores in `bnode_chunk' the pointer to the 
 * first bnode_chunk and returns a pointer to the bnode_hdr. On errors it 
 * returns a NULL pointer.
 */
bnode_hdr *tracer_build_bentry(void *void_map, void *void_node, bnode_chunk **bnodechunk, u_char level) 
{
	map_node  *int_map=(map_node *)void_map, *node=(map_node *)void_node;
	map_gnode **ext_map=(map_gnode **)void_map, *gnode=(map_gnode *)void_node;
	map_gnode *gn;
	bnode_hdr *bhdr;
	bnode_chunk *bchunk;
	int i, bm;
	u_char lvl;
	
	if(!(node->flags & MAP_BNODE))
		goto error;

	bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], void_map, 
			void_node, level);
	if(bm==-1)
		goto error;

	/*This will never happen, but we know the universe is fucking bastard*/
	if(!me.bnode_map[level][bm].links)
		goto error;

	bhdr=xmalloc(sizeof(bnode_hdr));
	bchunk=xmalloc(sizeof(bnode_chunk)*me.bnode_map[level][bm].links);
	memset(bhdr, 0, sizeof(bnode_hdr));
	memset(bchunk, 0, sizeof(bnode_chunk)*me.bnode_map[level][bm].links);

	if(!level)
		bhdr->bnode=pos_from_node(node, int_map);
	else
		bhdr->bnode=pos_from_gnode(gnode, ext_map[_EL(level)]);
	
	/* Fill the bnode chunks */
	for(i=0; i < me.bnode_map[level][bm].links; i++) {
		gn=(map_gnode *)me.bnode_map[level][bm].r_node[i].r_node;
		lvl=extmap_find_level(me.ext_map, gn, me.cur_quadg.levels);
		
		bchunk[i].gnode=pos_from_gnode(gn, me.ext_map[_EL(lvl)]);
		bchunk[i].level=lvl;
		memcpy(&bchunk[i].rtt, &me.bnode_map[level][bm].r_node[i].rtt, 
				sizeof(struct timeval));
		bhdr->links++;
	}

	*bnodechunk=bchunk;
	return bhdr;
error:
	*bnodechunk=0;
	return 0;
}

/* 
 * tracer_pack_pkt: do ya need explanation? pretty simple: pack the tracer packet
 */
char *tracer_pack_pkt(brdcast_hdr *bcast_hdr, tracer_hdr *trcr_hdr, tracer_chunk *tracer, 
		      bnode_hdr *bhdr, bnode_chunk *bchunk)
{
	size_t bblock_sz=0, pkt_sz;
	char *msg, *buf;

	if(bhdr && bchunk)
		bblock_sz=BNODEBLOCK_SZ(bhdr->links);

	pkt_sz=BRDCAST_SZ(TRACERPKT_SZ(trcr_hdr->hops))+bblock_sz;
	
	buf=msg=xmalloc(pkt_sz);
	memset(msg, 0, pkt_sz);

	/* add broadcast header */
	memcpy(buf, bcast_hdr, sizeof(brdcast_hdr));
	buf+=sizeof(brdcast_hdr);
	
	/* add tracer header */
	memcpy(buf, trcr_hdr, sizeof(tracer_hdr));
	buf+=sizeof(tracer_hdr);
	
	/* add the tracer chunks */
	memcpy(buf, tracer, sizeof(tracer_chunk)*trcr_hdr->hops);

	/* add the bnode chunks */
	if(bhdr && bchunk) {
		buf+=sizeof(tracer_chunk)*trcr_hdr->hops;
		memcpy(buf, bhdr, sizeof(bnode_hdr));
		buf+=sizeof(bnode_hdr);
		memcpy(buf, bchunk, sizeof(bnode_chunk)*bhdr->links);
	}

	return msg;
}

/* 
 * tracer_unpack_pkt: Given a packet `rpkt' it scomposes the rpkt.msg in 
 * `new_bcast_hdr', `new_tracer_hdr', `new_tracer', 'new_bhdr', and 
 * `new_block_sz'.
 * It returns 0 if the packet is valid, otherwise -1 is returned.
 */
int tracer_unpack_pkt(PACKET rpkt, brdcast_hdr **new_bcast_hdr, 
		      tracer_hdr **new_tracer_hdr, tracer_chunk **new_tracer, 
		      bnode_hdr **new_bhdr, size_t *new_bblock_sz)
{
	brdcast_hdr *bcast_hdr;
	tracer_hdr  *trcr_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	map_node *real_from;
	size_t bblock_sz=0, tracer_sz=0;
	int i, level, gid;

	bcast_hdr=(brdcast_hdr *)rpkt.msg;
	trcr_hdr=(tracer_hdr *)(rpkt.msg+sizeof(brdcast_hdr));
	tracer=(tracer_chunk *)(rpkt.msg+sizeof(brdcast_hdr)+sizeof(tracer_hdr));

	*new_bcast_hdr=0;
	*new_tracer_hdr=0;
	*new_tracer=0;
	*new_bhdr=0;
	*new_bblock_sz=0;

	tracer_sz=BRDCAST_SZ(TRACERPKT_SZ(trcr_hdr->hops));
	if(tracer_sz > rpkt.hdr.sz || !trcr_hdr->hops || 
			trcr_hdr->hops > MAXGROUPNODE) {
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
		return -1;
	}

	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=(bnode_hdr *)rpkt.msg+tracer_sz;
		if(!bhdr->links){
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
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

	/* 
	 * Now, let's check if we are part of the bcast_hdr->g_node of 
	 * bcast_hdr->level. If not let's  drop it! Why the hell this pkt is 
	 * here?
	 *
	 * TODO: something terrible happens: if this iptogid isn't called, the
	 * next iptogid call with me.cur_quadg.levels-1 will return a wrong
	 * gid!! DAMN!
	 */
	iptogid(rpkt.from, me.cur_quadg.levels);
	for(i=me.cur_quadg.levels-1; i>=0; i--) {
		gid=iptogid(rpkt.from, i);
		if(gid != me.cur_quadg.gid[i] && i > level) {
		debug(DBG_INSANE, "%s:%d", ERROR_POS);
			return -1;
		}
	}

	if(!level)
		iptomap((u_int)me.int_map, rpkt.from, me.cur_quadg.ipstart[1], (u_int *)&real_from);
	else {
		gid=iptogid(rpkt.from, level);
		real_from=&me.ext_map[_EL(level)][gid].g;
	}

	if(tracer_verify_pkt(tracer, trcr_hdr->hops, real_from, level)) {
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
 * bnode_block_start+bblock_sz for bnode block.
 * It puts the address of the found bblock_hdr in the `bbl_hdr' (bnode block list)
 * and the address pointing to the start of the bnode_chunk in the `bbl'. The 
 * total size of all the valid bblocks considered is stored in `*bblock_found_sz'.
 * It then returns the number of bblock found. Remember to xfree bbl_hdr and bbl
 * after using tracer_split_bblock. 
 */
u_short tracer_split_bblock(void *bnode_block_start, size_t bblock_sz, bnode_hdr ***bbl_hdr, 
		        bnode_chunk ****bbl, size_t *bblock_found_sz)
{
	bnode_hdr 	*bblock_hdr;
	bnode_chunk 	*bblock;
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	int e,p,x=0;
		
	*bblock_found_sz=0;
	if(!bblock_sz)
		return -1;
	
	for(e=0; e < bblock_sz; ) {
		bblock_hdr=bnode_block_start+e;
		bblock=(void *)bblock_hdr+sizeof(bnode_hdr);

		if(bblock_hdr->links <= 0) {
			e+=BNODEBLOCK_SZ(0);
			continue;
		}
		if(bblock_hdr->links >= MAXGROUPNODE)
			goto skip;

		/*Are we going far away the end of the buffer?*/
		if(bblock_sz-e < sizeof(bnode_hdr)+(sizeof(bnode_chunk)*bblock_hdr->links))
			break;
		
		bblist_hdr=xrealloc(bblist, sizeof(bnode_hdr *) * (x+1));
		bblist=xrealloc(bblist, sizeof(bnode_chunk *) * (x+1));

		bblist_hdr[x]=bblock_hdr;
		bblist[x]=xmalloc(sizeof(bnode_chunk *) * bblock_hdr->links);
		for(p=0; p<bblock_hdr->links; p++)
			bblist[x][p]=&bblock[p];
		
		(*bblock_found_sz)+=BNODEBLOCK_SZ(bblock_hdr->links);
		x++;
skip:
		e+=BNODEBLOCK_SZ(bblock_hdr->links);
	}
	
	*bbl_hdr=bblist_hdr;
	*bbl=bblist;
	return x;
}

/* tracer_store_pkt: This is the main function used to keep the int/ext_map's karma in peace.
 * It updates the `map' with the given `tracer' pkt. The bnode blocks (if any) are
 * unpacked and used to update the data of the boarding gnodes.
 * In `*bblocks_found' it stores the number of bblocks considered and stores in 
 * `bblocks_found_block' these bblocks. The `bblocks_found_block' will be appended
 * in the new tracer_pkt, after our bblock entry (if any). 
 * Remember to xfree(bblocks_found_block);
 */
int tracer_store_pkt(void *void_map, u_char level, tracer_hdr *trcr_hdr, tracer_chunk *tracer,
		     void *bnode_block_start, size_t bblock_sz, 
		     u_short *bblocks_found,  char **bblocks_found_block, size_t *bblock_found_sz)
{
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	struct timeval trtt;
	map_node  *int_map, *from, *node, *root_node;
	void *void_node;
	map_gnode **ext_map, *gfrom, *gnode;
	map_rnode rn;
	int i, e, diff, bm, x, f, from_rnode_pos, skip_rfrom;
	u_int hops;
	u_short bb;
	size_t found_block_sz;
	char *found_block;

	int_map = (map_node *)void_map;
	ext_map = (map_gnode **)void_map;
	hops = trcr_hdr->hops;

	/* Nothing to store */
	if(hops <= 1)
		return 0;
	
	if(!level) {
		ext_map        = me.ext_map;
	 	from   	       = node_from_pos(tracer[hops-1].node, int_map);
		root_node      = me.cur_node;
		from_rnode_pos = rnode_find(root_node, from);
	} else {
		gfrom	       = gnode_from_pos(tracer[hops-1].node, ext_map[_EL(level)]);
		from	       = &gfrom->g;
		root_node      = &me.cur_quadg.gnode[_EL(level)]->g;
		from_rnode_pos = rnode_find(root_node, from);
	}
	
	if(!(from->flags & MAP_RNODE)) { 
		/* 
		 * The `from' node isn't in our r_nodes, this means that it's a
		 * crap tracer pkt.
		 */
		return -1;
	}
	from->flags&=~QSPN_OLD;

	if(bblock_sz) {	/*Well, well, we have to take care of bnode blocks*/
		bb=tracer_split_bblock(bnode_block_start, bblock_sz, &bblist_hdr,
				&bblist, &found_block_sz);
		*bblocks_found = bb;
		if(!bb || bb!=trcr_hdr->bblocks) {
			debug(DBG_NORMAL, "tracer_store_pkt(): malformed bnode block.");
			*bblock_found_sz = 0;
			*bblocks_found_block = 0;
		} else {
			x=0;
			*bblock_found_sz=found_block_sz;
			*bblocks_found_block=found_block=xmalloc(found_block_sz);
			for(i=0; i<bb; i++) {
				memcpy(found_block+x, bblist_hdr[i], sizeof(bnode_hdr));
				x+=sizeof(bnode_hdr);
				if(!level) {
					node=node_from_pos(bblist_hdr[i]->bnode, int_map);
					node->flags|=MAP_BNODE;
					node->flags&=~QSPN_OLD;
					me.cur_node->flags|=MAP_BNODE;
					void_node=(void *)node;
				} else {
					gnode=gnode_from_pos(bblist_hdr[i]->bnode, ext_map[_EL(level)]);
					gnode->g.flags|=MAP_BNODE;
					gnode->g.flags&=~QSPN_OLD;
					me.cur_quadg.gnode[_EL(level)]->g.flags|=MAP_BNODE;
					void_node=(void *)&gnode->g;
				}

				/*Let's check if we have this bnode in the bmap, if not let's add it*/
				bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], 
						void_map, void_node, level);
				if(bm==-1)
					bm=map_add_bnode(&me.bnode_map[level], &me.bmap_nodes[level],
							bblist_hdr[i]->bnode,  bblist_hdr[i]->links);

				/*
				 * Now, we delete all the bnode's gnode_rnodes, then we store the 
				 * new ones. Yea, I know I'm raw, ahahaha.
				 */
				rnode_destroy(&me.bnode_map[level][bm]);

				/* We brought kaos, let's give peace */
				for(e=0; e<bblist_hdr[i]->links; e++) {
					memset(&rn, 0, sizeof(map_rnode));
					gnode=gnode_from_pos(bblist[i][e]->gnode, 
							ext_map[_EL(bblist[i][e]->level)]);
					gnode->g.flags&=~QSPN_OLD;

					rn.r_node=(u_int *)gnode;
					memcpy(&rn.rtt, &bblist[i][e]->rtt, sizeof(struct timeval));
					rnode_add(&me.bnode_map[level][bm], &rn);

					memcpy(found_block+x, &bblist_hdr[i][e], sizeof(bnode_chunk));
					x+=sizeof(bnode_chunk);
				}

				xfree(bblist[i]);
			}
			xfree(bblist_hdr);
			xfree(bblist);
		}
	}
	
	/* * Time to store the qspn routes to reach all the nodes of the tracer pkt * */
	
	/* We add in the total rtt the first rtt which is me -> from */
	memset(&trtt, 0, sizeof(struct timeval));	
	timeradd(&root_node->r_node[from_rnode_pos].trtt, &trtt, &trtt);
	
	/* We skip the node at hops-1 which it is the `from' node. The radar() 
	 * takes care of him. */
	skip_rfrom = !level ? 1 : 0;
	for(i=(hops-skip_rfrom)-1; i >= 0; i--) {
		timeradd(&tracer[i].rtt, &trtt, &trtt);

		if(!level) {
			node=node_from_pos(tracer[i].node, int_map);
			if(node == me.cur_node) {
				debug(DBG_INSANE, "Ehi! This is insane, there's "
						"a hop in the tracer pkt which "
						"points to me");
				continue;
			}
		} else {
			gnode=gnode_from_pos(tracer[i].node, ext_map[_EL(level)]);
			node=&gnode->g;
		}
		node->flags&=~QSPN_OLD;
			
		if(node->flags & MAP_VOID) { 
			/* Ehi, we hadn't this node in the map. Add it. */
			node->flags&=~MAP_VOID;
			node->flags|=MAP_UPDATE;
			
			if(level < GET_LEVELS(my_family)) {
				me.cur_quadg.gnode[_EL(level+1)]->seeds++;
				if( me.cur_quadg.gnode[_EL(level+1)]->seeds == MAXGROUPNODE )
					me.cur_quadg.gnode[_EL(level+1)]->flags|=GMAP_FULL;
			}
			debug(DBG_INSANE, "TRCR_STORE: node %d added", tracer[i].node);
		}
		
		for(e=0,f=0; e < node->links; e++) {
			if(node->r_node[e].r_node == (u_int *)from) {
				/* update the rtt of the node */
				diff=abs(MILLISEC(node->r_node[e].trtt) - MILLISEC(trtt));
				if(diff >= RTT_DELTA) {
					memcpy(&node->r_node[e].trtt, &trtt, sizeof(struct timeval));
					node->flags|=MAP_UPDATE;
				}
				f=1;
				break;
			}
		}
		if(!f) { 
			/*If the `node' doesn't have `from' in his r_nodes... let's add it*/
			map_rnode rnn;
			
			memset(&rnn, '\0', sizeof(map_rnode));
			rnn.r_node=(u_int *)from;
			memcpy(&rnn.trtt, &trtt, sizeof(struct timeval));
			
			rnode_add(node, &rnn);
			node->flags|=MAP_UPDATE;
		}

		/* ok, now the kernel needs a refresh in the route table */
		if(node->flags & MAP_UPDATE) {
			rnode_trtt_order(node);
			
			if(node->links > MAXROUTES) { 
				/* 
				 * If we have too many routes we purge the worst
				 * ones.
				 */
				diff=node->links - MAXROUTES;
				for(x=MAXROUTES; x < node->links; x++)
					rnode_del(node, x);
			}
			
			debug(DBG_INSANE, "TRCR_STORE: krnp_update node %d", tracer[i].node);
			krnl_update_node(node, level);
		}
	}
	return 0;
}


/* 
 * tracer_pkt_build: It builds a tracer_pkt and stores it in `pkt'.
 * If `trcr_hdr' or `tracer' are null, it will build a brand new tracer_pkt, otherwise
 * it will append in the `tracer' the new entry. Tracer_pkt_build will append also the old
 * bblock: `old_bchunks' is the number of bblocks, `old_bblock' is the block of the old 
 * bblock and it is `old_bblock_sz'. If `old_bchunks' is 0 or `old_bblock' is null or 
 * `old_bblock_sz' they are ignored.
 * The `pkt.hdr.op' is set to `rq', `pkt.hdr.id' to `rq_id' and the `bcast_hdr.sub_id' to
 * `bcast_sub_id'.
 * The packet shall be sent with tracer_pkt_send.
 * It returns -1 on errors. 
 */
int tracer_pkt_build(u_char rq,   	     int rq_id, 	     int bcast_sub_id,
		     int gnode_id,	     u_char gnode_level,
		     brdcast_hdr *bcast_hdr, tracer_hdr *trcr_hdr, tracer_chunk *tracer,  
		     u_short old_bchunks,    char *old_bblock,       size_t old_bblock_sz,  
		     PACKET *pkt)
{
	tracer_chunk *new_tracer=0;
	bnode_hdr    *new_bhdr=0;
	bnode_chunk  *new_bchunk=0;
	void *void_map, *void_node, *p;
	size_t new_bblock_sz=0;
	int new_tracer_pkt=0;
	u_int hops=0;

	if(!trcr_hdr || !tracer || !bcast_hdr) {
		/* Brand new tracer packet */
		new_tracer_pkt=1;
		bcast_hdr=xmalloc(sizeof(brdcast_hdr));
		memset(bcast_hdr, 0, sizeof(brdcast_hdr));
		
		bcast_hdr->gttl=1;
		bcast_hdr->level=gnode_level+1;
		bcast_hdr->g_node=gnode_id; 
		
		trcr_hdr=xmalloc(sizeof(tracer_hdr));
		memset(trcr_hdr, 0, sizeof(tracer_hdr));
		hops=0;
	} else {
		hops=trcr_hdr->hops;
	}

	memset(pkt, 0, sizeof(PACKET));
	pkt->hdr.op=rq;
	pkt->hdr.id=rq_id;
	pkt->hdr.flags|=BCAST_PKT;
	bcast_hdr->flags|=BCAST_TRACER_PKT;
	
	if(!gnode_level) {
		void_map=(void *)me.int_map;
		void_node=(void *)me.cur_node;
	} else {
		void_map=(void *)me.ext_map;
		void_node=(void *)me.cur_quadg.gnode[_EL(gnode_level)];
	}
	
	/* Time to append our entry in the tracer_pkt */
	new_tracer=tracer_add_entry(void_map, void_node, tracer, &hops, 
			gnode_level); 
	if(!hops)
		fatal("hops 0 in tracer_pkt_build! WTF!?!?!?");

	/* If we are a bnode we have to append the bnode_block too. */
	if(me.cur_node->flags & MAP_BNODE)
		if((new_bhdr=tracer_build_bentry(void_map, void_node, &new_bchunk,
						gnode_level))) {
			trcr_hdr->bblocks=new_bhdr->links;
			new_bblock_sz=BNODEBLOCK_SZ(new_bhdr->links);
		}

	/*
	 * If in the old tracer_pkt is present a bblock, we append it after the 
	 * new entry.
	 */
	if(old_bchunks && old_bblock && old_bblock_sz) {
		new_bblock_sz+=old_bblock_sz;
		new_bchunk=xrealloc(new_bchunk, new_bblock_sz);
	
		p=(void *)new_bchunk+BNODEBLOCK_SZ(new_bhdr->links);
		memcpy(p, old_bblock, old_bblock_sz);
		new_bhdr->links+=old_bchunks;
		trcr_hdr->bblocks=new_bhdr->links;
	}

	/* 
	 * Here we are really building the pkt packig all the stuff into a
	 * single bullet.
	 */
	trcr_hdr->hops=hops;
	bcast_hdr->sub_id=bcast_sub_id;
	bcast_hdr->sz=TRACERPKT_SZ(hops)+new_bblock_sz;
	pkt->hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	
	pkt->msg=tracer_pack_pkt(bcast_hdr, trcr_hdr, new_tracer, new_bhdr, 
			new_bchunk);
	
	/* Yea, finished */
	if(new_tracer)
		xfree(new_tracer);	
	if(new_bhdr)
		xfree(new_bhdr);
	if(new_bchunk)
		xfree(new_bchunk);
	if(new_tracer_pkt) {
		xfree(bcast_hdr);
		xfree(trcr_hdr);
	}
	return 0;
}

/* 
 * tracer_pkt_send: It sends only a normal tracer_pkt which is packed in `pkt'. 
 * It sends the `pkt' to all the nodes excluding the excluded nodes. It knows 
 * if a node is excluded by calling the `is_node_excluded' function. The 
 * second argument to this function is the node who sent the pkt and it must be
 * always excluded. The third argument is the position of the node being processed
 * in the r_node array of me.cur_node. The other arguments are described in
 * tracer.h.
 * If `is_node_excluded' returns a non 0 value, the node is considered as excluded.
 * The `from' argument to tracer_pkt_send is the node who sent the `pkt'.
 * It returns the number of pkts sent or -1 on errors. Note that the total pkt sent
 * should be == me.cur_node->links-1.
 * Note that `gid', `level', `sub_id' and `from' are vars used only by
 * is_node_excluded().
 */
int tracer_pkt_send(int(*is_node_excluded)(TRACER_PKT_EXCLUDE_VARS), int gid, 
		u_char level, int sub_id, map_node *from, PACKET pkt)
{
	inet_prefix to;
	map_node *dst_node;
	ssize_t err;
	const char *ntop;
	int i, e=0;

	/*Forward the pkt to all our r_nodes (excluding the excluded;)*/
	for(i=0; i<me.cur_node->links; i++) {
		dst_node=(map_node *)me.cur_node->r_node[i].r_node;
		if(is_node_excluded(dst_node, from, i, gid, level, sub_id))
			continue;

		/* We need the ip of the rnode ;^ */
		rnodetoip((u_int)me.int_map, (u_int)dst_node, 
				me.cur_quadg.ipstart[1], &to);
		
		debug(DBG_INSANE, "tracer_pkt_send(): %s to %s", 
				rq_to_str(pkt.hdr.op), inet_to_str(to));
				
		pkt_addto(&pkt, &to);
		pkt.sk_type=SKT_UDP;

		/*Let's send the pkt*/
		err=send_rq(&pkt, 0, pkt.hdr.op, pkt.hdr.id, 0, 0, 0);
		if(err==-1) {
			ntop=inet_to_str(pkt.to);
			error("tracer_pkt_send(): Cannot send the %s request"
					" with id: %d to %s.", rq_to_str(pkt.hdr.op),
					pkt.hdr.id, ntop);
		} else
			e++;
	}
		
	pkt_free(&pkt, 1);
	return e;
}

/* * * these exclude function are used in conjunction with tracer_pkt_send.* * */

/*
 * exclude_glevel: Exclude `node' if it doesn't belong to the gid (`excl_gid') of 
 * the level (`excl_level') specified.
 */
int exclude_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	map_bnode *bnode;
	int i, level, gid;
	
	if((node->flags & MAP_GNODE || node->flags & MAP_ERNODE) && 
			excl_level != 0) {
		bnode=node;
		/* 
		 * If the bnode is near at least with one gnode included in the
		 * excl_gid of excl_level we can continue to forward the pkt
		 */
		for(i=0; i<bnode->links; i++) {
			level=extmap_find_level(me.ext_map, 
					(map_gnode *)bnode->r_node[i].r_node,
					me.cur_quadg.levels);
			gid=pos_from_gnode((map_gnode *)bnode->r_node[i].r_node,
					me.ext_map[_EL(level)]);
			if(level < excl_level || ((level == excl_level) && 
						(gid==excl_gid)))
				return 0;
		}
		return 1;
	}
	return 0;
}

/* Exclude the `from' node */
int exclude_from(TRACER_PKT_EXCLUDE_VARS)
{
	if(node == from)
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
 * Exclude the from node, the node's gnode of a higher level, and set the
 * QSPN_REPLIED flag.
 */
int exclude_from_glevel_and_setreplied(TRACER_PKT_EXCLUDE_VARS)
{
	if(exclude_glevel(TRACER_PKT_EXCLUDE_VARS_NAME) || 
			exclude_from(TRACER_PKT_EXCLUDE_VARS_NAME))
		return 1;

	node->flags|=QSPN_REPLIED;
	return 0;
}

int exclude_from_and_glevel_and_closed(TRACER_PKT_EXCLUDE_VARS)
{
	if((node->flags & QSPN_CLOSED) || 
			exclude_from_and_glevel(TRACER_PKT_EXCLUDE_VARS_NAME))
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
	map_node *from, *tracer_starter;
	map_gnode *gfrom;
	int ret_err;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	int gid;
	u_char level, orig_lvl;
	const char *ntop;
	char *old_bblock;
	void *void_map;

	debug(DBG_INSANE, "Tracer_pkt(0x%x) received.", rpkt.hdr.id);
	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &trcr_hdr, &tracer, &bhdr, &bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "tracer_pkt_recv(): The %s sent an invalid tracer_pkt here.", ntop);
		return -1;
	}

	hops=trcr_hdr->hops;
	gid=bcast_hdr->g_node;
	level=orig_lvl=bcast_hdr->level;
	if(!level || level == 1) {
		level=0;
		from=node_from_pos(tracer[hops-1].node, me.int_map);
		tracer_starter=node_from_pos(tracer[0].node, me.int_map);
		void_map=me.int_map;
	} else {
		level--;
		gfrom=gnode_from_pos(tracer[hops-1].node, me.ext_map[_EL(level)]);
		from=&gfrom->g;
		tracer_starter=(map_node *)gnode_from_pos(tracer[0].node, 
				me.ext_map[_EL(level)]);
		void_map=me.ext_map;
	}

	/*
	 * This is the check for the broadcast id. If it is <= tracer_starter->brdcast
	 * the pkt is an old broadcast that still dance around.
	 */
	if(rpkt.hdr.id <= tracer_starter->brdcast) {
		ntop=inet_to_str(rpkt.from);
		debug(DBG_NOISE, "tracer_pkt_recv(): Received from %s an old "
				"tracer_pkt broadcast: %d", ntop, rpkt.hdr.id);
		return -1;
	} else
		tracer_starter->brdcast=rpkt.hdr.id;


	/* 
	 * Time to update our map
	 */
	if(rpkt.hdr.op == TRACER_PKT) { /*This check is made because tracer_pkt_recv 
					 handles also TRACER_PKT_CONNECT pkts*/
		
		ret_err=tracer_store_pkt(void_map, level, trcr_hdr, tracer, 
				(void *)bhdr, bblock_sz, &old_bchunks, 
				&old_bblock, &old_bblock_sz);
		if(ret_err) {
			ntop=inet_to_str(rpkt.from);
			debug(DBG_NORMAL, "tracer_pkt_recv(): Cannot store the"
					" tracer_pkt received from %s", ntop);
		}
	}

	/*The forge of the packet.*/
	tracer_pkt_build(rpkt.hdr.op, rpkt.hdr.id, bcast_hdr->sub_id, /*IDs*/
			 gid,         level,			      /*GnodeID and level (ignored)*/
			 bcast_hdr,   trcr_hdr, tracer, 	      /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz,      /*bnode_block*/
			 &pkt);					      /*Where the pkt is built*/
	if(old_bblock)
		xfree(old_bblock);
	/*... forward the tracer_pkt to our r_nodes*/
	tracer_pkt_send(exclude_from_and_glevel, gid, orig_lvl, -1, from, pkt);
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
	map_node *from=me.cur_node;
	int root_node_pos;
	
	if(tracer_pkt_start_mutex)
		return 0;
	else
		tracer_pkt_start_mutex=1;

	if(!level || level == 1)
		root_node_pos=pos_from_node(me.cur_node, me.int_map);
	else
		root_node_pos=pos_from_gnode(me.cur_quadg.gnode[_EL(level)], 
				me.ext_map[_EL(level)]);

	me.cur_node->brdcast++;
	tracer_pkt_build(TRACER_PKT, me.cur_node->brdcast, root_node_pos,/*IDs*/
			 me.cur_quadg.gid[level],	   level,	 /*GnodeID and level*/
			 0,          0,                    0, 		 /*Received tracer_pkt*/
			 0,          0,                    0, 		 /*bnode_block*/
			 &pkt);						 /*Where the pkt is built*/
	/*Diffuse the packet in all the universe!*/
	debug(DBG_INSANE, "Tracer_pkt(0x%x) starting.", pkt.hdr.id);
	tracer_pkt_send(exclude_from_and_glevel, me.cur_quadg.gid[level], 
			level+1, -1, from, pkt);
	tracer_pkt_start_mutex=0;
	return 0;
}

/* Note: the tracer_pkt_connect is valid only in our gnode */
int tracer_pkt_connect(map_node *dst)
{
	PACKET pkt;
	map_node *from=me.cur_node;
	brdcast_hdr bcast_hdr;
	tracer_hdr trcr_hdr;
	tracer_chunk tracer[2];
	u_char gid=me.cur_quadg.gid[1], level=0; 	
	int root_node_pos;

	trcr_hdr.hops=2;
	trcr_hdr.bblocks=0;
	memset(&tracer[0], 0, sizeof(tracer_chunk));
	memset(&tracer[1], 0, sizeof(tracer_chunk));
	tracer[0].node=pos_from_node(me.cur_node, me.int_map);
	tracer[1].node=pos_from_node(dst, me.int_map);
	memset(&bcast_hdr, 0, sizeof(brdcast_hdr));
	bcast_hdr.g_node=gid;
	bcast_hdr.gttl=1;
	bcast_hdr.sz=sizeof(tracer_hdr)+(sizeof(tracer_chunk)*2);
	
	me.cur_node->brdcast++;
	root_node_pos=pos_from_node(me.cur_node, me.int_map);
	tracer_pkt_build(TRACER_PKT_CONNECT, me.cur_node->brdcast, root_node_pos,/*IDs*/
			 gid,		     level,
			 &bcast_hdr,         &trcr_hdr,          tracer,        /*Received tracer_pkt*/
			 0,                  0,              0, 		/*bnode_block*/
			 &pkt);				 			/*Where the pkt is built*/

	/*Diffuse the packet in all the universe!*/
	tracer_pkt_send(exclude_from_and_glevel, gid, level, -1, from, pkt);
	return 0;
}
