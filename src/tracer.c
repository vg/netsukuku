#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#include "tracer.h"
#include "misc.h"
#include "log.h"
#include "xmalloc.h"

extern int my_family;
extern struct current me;

/* tracer_verify_pkt: It checks the validity of `tracer': The last entry
 * in the tracer must be a node present in our r_nodes. If `real_from'
 * is non 0, it checks if it is equal to the last entry in the packet. */
int tracer_verify_pkt(tracer_chunk *tracer, u_int hops, map_node *real_from, u_char level)
{
	map_node *from;
	int e, x=0;

	/*"from" has to be absolutely one of our rnodes*/
	if(!level) {
		from=node_from_pos(tracer[hops-1].node, me.int_map);
		if(from && from != real_from)
			return -1;
		if(rnode_find(me.cur_node, from) == -1)
			return -1;
	} else {
		from=gnode_from_pos(me.cur_quadg.gid[level], me.ext_map[_EL(level)]);
		if(from && (void *)from != (void *)real_from) 
			return -1;
		if(g_rnode_find(me.cur_quag.gnode[_EL(level)], (map_gnode *)from) == -1)
			return -1;
	}
	return 0;
}

/* tracer_add_entry: Add our entry `node' to the tracer pkt `tracer' wich has `hops'.
 * It returns the modified tracer pkt in a newly mallocated struct and it increments the `*hops'.
 * If `tracer' is null it will return the new tracer_pkt.
 * On errors it returns NULL.*/
tracer_chunk *tracer_add_entry(void *void_map, void *void_node, tracer_chunk *tracer, u_int *hops, u_char level)
{
	tracer_chunk *t;
	map_node *from;
	map_rnode *rfrom=0;
	map_node  *map=(map_node *)void_map, *node=(map_node *)void_node;
	map_gnode **ext_map=(map_gnode *)void_map, *gnode=(map_gnode *)void_node;
	int pos;

	*hops++;
	t=xmalloc(sizeof(tracer_chunk)*hops);
	memset(t, 0, sizeof(tracer_chunk)*hops);
	if(tracer || *hops) {
		memcpy(t, tracer, sizeof(tracer_chunk) * (*hops-1));
		if(!level) {
			from=node_from_pos(tracer[hops-1].node, map);
			if((pos=rnode_find(me.cur_node, from)) == -1)
				return 0;
			else
				rfrom=&me.cur_node->r_node[pos];
		} else {
			from=gnode_from_pos(tracer[hops-1].node, ext_map[_EL(level)]);
			if((pos=g_rnode_find(me.cur_quag.gnode[_EL(level)], (map_gnode *)from) == -1))
				return 0;
			else
				rfrom=me.cur_quag.gnode[_EL(level)]->g.r_node[pos]
		}
	}

	if(!level)
		t[hops-1].node=pos_from_node(node, map);
	else
		t[hops-1].node=pos_from_gnode(gnode, ext_map[_EL(level)]);
		
	if(rfrom)
		memcpy(&t[hops-1].rtt, &rfrom->rtt, sizeof(struct timeval));
	return t;
}

/* tracer_build_bentry: It builds the bnode_block to be added in the bnode's entry in 
 * the tracer pkt. It stores in `bnode_chunk' the pointer to the first bnode_chunk and
 * returns a pointer to the bnode_hdr. On errors it returns a NULL pointer.*/
bnode_hdr *tracer_build_bentry(void *void_map, void *void_node, bnode_chunk *bnode_chunk, u_char level) 
{
	map_node  *int_map=(map_node *)void_map, *node=(map_node *)void_node;
	map_gnode **ext_map=(map_gnode *)void_map, *gnode=(map_gnode *)void_node;
	bnode_hdr *bhdr;
	bnode_chunk *bchunk;
	int i, bmi;
	u_char lvl;
	
	if(!(node->flags & MAP_BNODE))
		goto error;
	
	if((bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], void_map, void_node, level))==-1) 
		goto error;

	/*This will never happen, but we know the universe is fucking bastard*/
	if(!me.bnode_map[level][bm].links)
		goto error;

	bhdr=xmalloc(sizeof(bnode_hdr));
	bchunk=xmalloc(sizeof(bnode_chunk)*me.bnode_map[level][bm].links);
	memset(bhdr, 0, sizeof(bnode_hdr));
	memset(bchunk, 0, sizeof(bnode_chunk)*me.bnode_map[level][bm].links);

	if(!level) {
		bhdr->bnode=pos_from_node(node, map);
	else
		bhdr->bnode=pos_from_gnode(gnode, ext_map[_EL(level)]);
	for(i=0; i<me.bnode_map[level][bm].links; i++) {
		lvl=extmap_find_level(me.ext_map, me.bnode_map[level][bm].r_node[i].r_node, me.cur_quadg.levels);
		bchunk[i].gnode=pos_from_gnode(me.bnode_map[level][bm].r_node[i].r_node, me.ext_map[_EL(lvl)]);
		bchunk[i].level=lvl;
		memcpy(&bchunk[i].rtt, &me.bnode_map[level][bm].r_node[i].rtt, sizeof(struct timeval));
		bhdr->links++;
	}

	*bnode_chunk=bchunk;
	return bhdr;
error:
	*bnode_chunk=0;
	return 0;
}

/* tracer_pack_pkt: do ya need explanation? pretty simple: pack the tracer packet*/
char *tracer_pack_pkt(brdcast_hdr *bcast_hdr, tracer_hdr *tracer_hdr, tracer_chunk *tracer, 
		      bnode_hdr *bhdr, bnode_chunk *bchunk)
{
	size_t bblock_sz=0, pkt_sz;
	char *msg, *buf;

	if(bhdr && bchunk)
		bblock_sz=BNODEBLOCK_SZ(bhdr->links);

	pkt_sz=BRDCAST_SZ(TRACERPKT_SZ(hops))+bblock_sz;
	
	buf=msg=xmalloc(pkt_sz);
	memset(msg, 0, pkt_sz);

	memcpy(buf, bcast_hdr, sizeof(brdcast_hdr));
	buf+=sizeof(brdcast_hdr);
	memcpy(buf, tracer_hdr, sizeof(tracer_hdr));
	buf+=sizeof(tracer_hdr);
	memcpy(buf, tracer, sizeof(tracer_chunk)*tracer_hdr->hops);
	if(bhdr && bchunk) {
		buf+=sizeof(tracer_chunk)*tracer_hdr->hops;
		memcpy(buf, bhdr, sizeof(bnode_hdr));
		buf+=sizeof(bnode_hdr);
		memcpy(buf, bchunk, sizeof(bnode_chunk)*bhdr->links);
	}

	return msg;
}

/* tracer_unpack_pkt: Given a packet `rpkt' it scomposes the rpkt.msg in 
 * `new_bcast_hdr', `new_tracer_hdr', `new_tracer', 'new_bhdr', and 
 * `new_block_sz'.
 * It returns 0 if the packet is valid, otherwise -1 is returned.*/
int tracer_unpack_pkt(PACKET rpkt, brdcast_hdr *new_bcast_hdr, tracer_hdr *new_tracer_hdr, tracer_chunk *new_tracer, 
		      bnode_hdr *new_bhdr, size_t *new_bblock_sz)
{
	brdcast_hdr *bcast_hdr=rpkt.msg;
	tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(brdcast_hdr);
	tracer_chunk *tracer=rpkt.msg+sizeof(brdcast_hdr)+sizeof(tracer_hdr);
	bnode_hdr    *bhdr=0;
	map_node *real_from;
	size_t bblock_sz=0, tracer_sz=0;
	u_char level, i, gid;

	*new_bcast_hdr=*new_tracer_hdr=*new_tracer=*new_bhdr=*new_bblock_sz=0; /*Woa, I love the beauty of this string*/
	
	tracer_sz=(sizeof(tracer_hdr)+(tracer_hdr->hops*sizeof(tracer_chunk))+sizeof(brdcast_hdr));
	
	if(tracer_sz > rpkt.hdr.sz || !tracer_hdr->hops)
		return -1;

	if(rpkt.hdr.sz > tracer_sz) {
		bblock_sz=rpkt.hdr.sz-tracer_sz;
		bhdr=rpkt.msg+tracer_sz;
		if(!bhdr->links)
			return -1;
	}

	level=bcast_hdr->level;
	if(level==1)
		level=0;
	if(!(rpkt.hdr.flags & BCAST_PKT) || !(bcast_hdr->flags & BCAST_TRACER_PKT) || 
			level > GET_LEVELS(rpkt.from.family))
		return -1;

	/* Now, let's check if we are part of the bcast_hdr->g_node of bcast_hdr->level. If not let's 
	 * drop it! Why the hell this pkt is here?*/
	for(i=me.cur_quadg.levels-1; i!=0; i++) {
		gid=iptogid(bcast_hdr->g_ipstart, i);
		if(gid != me.cur_quadg.gid[i] && !(i < level))
			return -1;
	}

	if(!level)
		iptomap(me.int_map, rpkt.from, me.cur_quadg.ipstart[1], &real_from);
	else
		real_from=iptogid(rpkt.from, level);

	if(tracer_verify_pkt(tracer, tracer_hdr->hops, real_from, level, level))
		return -1;
	
	*new_bcast_hdr=bcast_hdr;
	*new_tracer_hdr=tracer_hdr;
	*new_tracer=tracer;
	*new_bhdr=bhdr;
	*new_bblock_sz=bblock_sz;
	return 0;
}

/* tracer_split_bblock: It searches from bnode_block_start to bnode_block_start+bblock_sz for bnode block.
 * It puts the address of the found bblock_hdr in the `bbl_hdr' (bnode block list) and the address
 * pointing to the start of the bnode_chunk in the `bbl'. The total size of all the valid bblocks considered
 * is stored in `*bblock_found_sz'.
 * It then returns the number of bblock found. Remember to xfree bbl_hdr and bbl 
 * after using tracer_split_bblock. */
u_short tracer_split_bblock(void *bnode_block_start, size_t bblock_sz, bnode_hdr *bbl_hdr, 
		        bnode_chunk *bbl, size_t *bblock_found_sz)
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
		if(bblist_hdr->links >= MAXGROUPNODE || bblist_hdr->bnode < 0)
			goto skip;

		/*Are we going far away the end of the buffer?*/
		if(bblock_sz-e < sizeof(bnode_hdr)+(sizeof(bnode_chunk)*bblock_hdr->links))
			break;
		
		bblist_hdr=xrealloc(bblist, sizeof(bnode_hdr *) * (x+1));
		bblist=xrealloc(bblist, sizeof(bnode_chunk *) * (x+1));

		bblist_hdr[x]=bblock_hdr;
		bblist[x]=xmalloc(sizeof(bnode_chunk *) * bblock_hdr->links);
		for(p=0; p<bblock_hdr->links; p++)
			bblist[x][p]=bblock[p];
		
		*bblock_found_sz+=BNODEBLOCK_SZ(bblock_hdr->links);
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
int tracer_store_pkt(void *void_map, u_char level, tracer_hdr *tracer_hdr, tracer_chunk *tracer,
		     void *bnode_block_start, size_t bblock_sz, 
		     u_short *bblocks_found,  char *bblocks_found_block, size_t *bblock_found_sz)
{
	bnode_hdr 	*bblock_hdr;
	bnode_chunk 	*bblock;	
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	struct timeval trtt;
	map_node  *int_map=(map_node *)void_map, *from, *node, *root_node;
	void *void_node;
	map_gnode **ext_map=(map_gnode *)void_map, *gfrom, *gnode;
	map_rnode rn;
	int i, e, diff, bm, x, rnode_pos, skip_rfrom;
	u_int hops=tracer_hdr->hops;
	u_short bb;
	size_t found_block_sz;
	char *found_block;
	u_char level;

	if(!level) {
		ext_map=me.ext_map;
	 	from=node_from_pos(tracer[hops-1].node, int_map);
		root_node=me.cur_node;
		rnode_pos=rnode_find(root_node, from);
	} else {
		from=gfrom=gnode_from_pos(tracer[hops-1].node, ext_map[_EL(level)]);
		root_node=&me.cur_quadg.gnode[_EL(level)].g;
		rnode_pos=rnode_find(root_node, from);
	}
	
	if(!(from.flags & MAP_RNODE)) { /*the `from' node isn't in our r_nodes. Add it!*/
		map_rnode rnn;
		memset(&rnn, '\0', sizeof(map_rnode));

		rnn.r_node=from;
		memcpy(&rnn.rtt, &tracer[hops-1].rtt, sizeof(struct timeval));
		rnode_add(root_node, &rnn);

		if(level)
			from->flags|=MAP_GNODE | MAP_BNODE;
		from->flags|=MAP_RNODE;
		from->flags&=~MAP_VOID & ~MAP_UPDATE;
	}

	if(bblock_sz) {	/*Well, well, we have to take care of bnode blocks*/
		bb=tracer_split_bblock(bnode_block_start, bblock_sz, &bbl_hdr, &bbl, &found_block_sz);
		*bblocks_found=bb;
		if(!bb || bb!=tracer_hdr->bblocks) {
			error("tracer_store_pkt(): malformed bnode block.");
			*bblock_found_sz=0;
			*bblocks_found_block=0;
		} else {
			x=0;
			*bblock_found_sz=found_block_sz;
			*bblocks_found_block=found_block=xmalloc(found_block_sz);
			for(i=0; i<bb; i++) {
				memcpy(found_block+x, bblist_hdr[i], sizeof(bnode_hdr));
				x+=sizeof(bnode_hdr);
				if(!level) {
					node=node_from_pos(bblist_hdr[i]->bnode, map);
					node->flags|=MAP_BNODE;
					me.cur_node|=MAP_BNODE;
					void_node=(void *)node;
				} else {
					gnode=gnode_from_pos(bblist_hdr[i]->bnode, ext_map[_EL(level)]);
					gnode->g.flags|=MAP_BNODE;
					me.cur_quadg.gnode[_EL(level)]->g.flags|=MAP_BNODE;
					void_node=(void *)&gnode.g;
				}

				/*Let's check if we have this bnode in the bmap, if not let's add it*/
				bm=map_find_bnode(me.bnode_map[level], me.bmap_nodes[level], void_map, void_node, level);
				if(bm==-1)
					bm=map_add_bnode(&me.bmap_nodes[level], &me.bmap_nodes[level], bblist_hdr[i]->bnode, 
							bblist_hdr[i]->links);

				/*Now, we delete all the bnode's gnode_rnodes, then we store the new ones.
				 * yea, I know I'm raw, ahahaha.*/
				rnode_destroy(&me.bnode_map[level][bm]);

				/*We brought kaos, let's give peace*/
				for(e=0; e<bblist_hdr[i]->links; e++) {
					memset(&rn, 0, sizeof(map_rnode));
					rn.r_node=gnode_from_pos(bblist[i][e]->gnode, ext_map[_EL(bblist[i][e]->level)]);
					memcpy(&rn.rtt, &bblist[i][e]->rtt, sizeof(struct timeval));
					rnode_add(&me.bnode_map[level][bm], &rn);

					memcpy(found_block+x, bblist_hdr[i][e], sizeof(bnode_chunk));
					x+=sizeof(bnode_chunk);
				}

				xfree(bblist[i]);
			}
			xfree(bblist_hdr);
			xfree(bblist);
		}
	}
	
	/* * Time to store the qspn routes to reach all the nodes of the tracer pkt * */
	
	timeradd(&root_node->r_node[rnode_pos].trtt, &trtt, &trtt);
	/* We skip the node at hops-1 which it is the `from' node. The radar() takes care of him.*/
	if(!level)
		skip_rfrom=1;
	else
		skip_rfrom=0;
	for(i=(hops-skip_rfrom)-1; i != 0; i--) {
		timeradd(&tracer[i].rtt, &trtt, &trtt);

		if(!level)
		node=node_from_pos(tracer[i].node, map);
		else {
			gnode=gnode_from_pos(tracer[i].node, ext_map[_EL(level)]);
			node=&gnode->g;
		}
			
		if(!(node->flags & MAP_VOID)) { /*Ehi, we hadn't this node in the map. Add it.*/
			from->flags&=~MAP_VOID;
			from->flags|=MAP_UPDATE;
			if(level < GET_LEVELS(my_family))
				if( (me.cur_quadg.gnode[_EL(level+1)]->seeds++) == MAXGROUPNODES )
					me.cur_quadg.gnode[_EL(level+1)]->flags|=GMAP_FULL;
		}
		for(e=0; e < node->links; e++) {
			if(node->r_node[e].r_node == from) {
				diff=abs(MILLISEC(node->r_node[e].trtt) - MILLISEC(trtt));
				if(diff >= RTT_DELTA) {
					memcpy(&node->r_node[e].trtt, &trtt, sizeof(struct timeval));
					node->flags|=MAP_UPDATE;
				}
				f=1;
			}
		}
		if(!f) { /*If the `node' doesn't have `from' in his r_nodes... let's add it*/
			map_rnode rnn;

			memset(&rnn, '\0', sizeof(map_rnode));
			rnn.r_node=from;
			memcpy(&rnn.trtt, &trtt, sizeof(struct timeval));
			rnode_add(node, &rnn);
			node->flags|=MAP_UPDATE;
		}

		/*ok, now the kernel needs a refresh*/
		if(node->flags & MAP_UPDATE) {
			rnode_trtt_order(node);
			if(node->links > MAXROUTES) { /*If we have too many routes we purge the worst ones*/
				diff=node->links - MAXROUTES;
				for(x=MAXROUTES; x < node->links; x++)
					rnode_del(node, x);
			}
			krnl_update_node(node, level);
		}
	}
	return 0;
}


/* tracer_pkt_build: It builds a tracer_pkt and stores it in `pkt'.
 * If `tracer_hdr' or `tracer' are null, it will build a brand new tracer_pkt, otherwise
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
		     brdcast_hdr *bcast_hdr, tracer_hdr *tracer_hdr, tracer_chunk *tracer,  
		     u_short old_bchunks,    char *old_bblock,       size_t old_bblock_sz,  
		     PACKET *pkt)
{
	tracer_chunk *new_tracer=0;
	bnode_hdr    *new_bhdr=0;
	bnode_chunk  *new_bchunk=0;
	void *void_map, *void_node;
	int new_tracer_pkt=0, ret=0;
	u_int hops=0;

	if(!tracer_hdr || !tracer || !bcast_hdr) {
		new_tracer_pkt=1;
		bcast_hdr=xmalloc(sizeof(brdcast_hdr));
		memset(bcast_hdr, 0, sizeof(brdcast_hdr));
		bcast_hdr.g_node=gnode_id; 
		bcast_hdr.level=gnode_level+1;
		memcpy(&bcast_hdr.g_ipstart, &me.cur_ip, sizeof(inet_prefix));
		bcast_hdr.gttl=1;
		tracer_hdr=xmalloc(sizeof(tracer_hdr));
		memset(tracer_hdr, 0, sizeof(tracer_hdr));
		hops=0;
	} else {
		hops=tracer_hdr->hops;
	}

	memset(&pkt, 0, sizeof(PACKET));
	pkt->hdr.id=rq_id;
	pkt->hdr.op=rq;
	pkt->hdr.flags|=BCAST_PKT;
	bcast_hdr.flags|=BCAST_TRACER_PKT;
	
	if(!level) {
		void_map=(void *)me.int_map;
		void_node=(void *)me.cur_node;
	} else {
		void_map=(void *)me.ext_map;
		void_node=(void *)me.cur_quadg.gnode[_EL(gnode_level)];
	}
	
	/*Time to append our entry in the tracer_pkt*/
	new_tracer=tracer_add_entry(void_map, void_node, tracer, &hops, gnode_level); 
	/*If we are a bnode we have to append the bnode_block too.*/
	if(me.cur_node->flags & MAP_BNODE)
		if((new_bhdr=tracer_build_bentry(void_map, void_node, &new_bchunk, gnode_level)))
			tracer_hdr->bblocks=new_bhdr->links;

	/*If in the old tracer_pkt is present a bblock, we append it after the new entry*/
	if(old_bchunks && old_bblock && old_bblock_sz) {
		new_bchunk=xrealloc(new_bchunk, (BNODEBLOCK_SZ(new_bhdr->links)+old_bblock_sz));
		memcpy((void *)new_bchunk+BNODEBLOCK_SZ(new_bhdr->links), old_bblock, old_bblock_sz);
		tracer_hdr->bblocks=new_bhdr->links+=old_bchunks;
	}

	/*Here we are building the pkt to... */
	memset(&pkt, '\0', sizeof(PACKET));
	bcast_hdr->sz=TRACERPKT_SZ(hops)+(tracer_hdr->bblocks ? BNODEBLOCK_SZ(tracer_hdr->bblocks) : 0);
	bcast_hdr->sub_id=bcast_sub_id;
	pkt->hdr.sz=BRDCAST_SZ(bcast_hdr->sz);
	tracer_hdr->hops=hops;
	pkt->msg=tracer_pack_pkt(bcast_hdr, tracer_hdr, new_tracer, new_bhdr, new_bchunk);
	
	if(new_tracer)
		xfree(new_tracer);	
	if(new_bhdr)
		xfree(new_bhdr);
	if(new_bchunk)
		xfree(new_bchunk);
	if(new_tracer_pkt) {
		xfree(bcast_hdr);
		xfree(tracer_hdr);
	}
	return 0;
}
/* is_node_excluded: is the `node' included in the `excluded_nodes' list?
int is_node_excluded(map_node *node, map_node **excluded_nodes, int exclusions)
{
	int i;
	
	if(exclusions <= 0)
		return 0;
	for(i=0; i<exclusions; i++) {
		if(node==excluded_nodes[i])
			return 1;
	}
	return 0;
}
*/

/* tracer_pkt_send: It sends only a normal tracer_pkt. This is useful after the hook,
 * to let all the other nodes know we are alive and to give them the right route.
 * It sends the `pkt' to all the nodes excluding the excluded nodes. It knows if a node
 * is excluded by calling the `is_node_excluded' function. The second argument to this
 * function is the node who sent the pkt and it must be always excluded. The 
 * third argument is the position of the node being processed in the r_node array 
 * of me.cur_node.
 * If `is_node_excluded' returns a non 0 value, the node is considered as excluded.
 * The `from' argument to tracer_pkt_send is the node who sent the `pkt'.
 * It returns the number of pkts sent or -1 on errors. Note that the total pkt sent
 * should be == me.cur_node->links-1*/
int tracer_pkt_send(int(*is_node_excluded)(TRACER_PKT_EXCLUDE_VARS), int gid, u_char level, map_node *from, PACKET pkt)
{
	inet_prefix to;
	map_node *dst_node;
	ext_rnode *e_rnode;
	ssize_t err;
	char *ntop;
	int i, e=0;

	/*Forward the pkt to all our r_nodes (excluding the excluded hehe;)*/
	for(i=0; i<me.cur_node->links; i++) {
		dst_node=(map_node *)me.cur_node->r_node[i].r_node;
		if(is_node_excluded(dst_node, from, i, gid, level))
			continue;

		memset(&to, 0, sizeof(inet_prefix));
		if(dst_node & MAP_ERNODE) {
			e_rnode=dst_node;
			memcpy(&to, &e_node->ip, sizeof(inet_prefix));
		} else 
			maptoip(me.int_map, dst_node, me.cur_quadg.ipstart[1], &to);
		pkt_addto(&pkt, &to);
		pkt.sk_type=SKT_UDP;

		/*Let's send the pkt*/
		err=send_rq(&pkt, 0, QSPN_CLOSE, rpkt.hdr.id, 0, 0, 0);
		if(err==-1) {
			ntop=inet_to_str(&pkt->to);
			error("tracer_pkt_send(): Cannot send the %s request with id: %d to %s.", rq_to_str(pkt.hdr.op),
					pkt.hdr.id, ntop);
			xfree(ntop);
		} else {
			e++;
		}
	}
		
	pkt_free(&pkt, 1);
	return e;
}

/* * * these exclude function are used in conjunction with tracer_pkt_send.* * */

/* exclude_glevel: Exclude `node' if it doesn't belong to the gid (`excl_gid') of 
 * the level (`excl_level') specified.*/
int exclude_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	map_bnode *bnode;
	int i, level, gid;
	
	if((dst_node->flags & MAP_GNODE || dst_node->flags & MAP_ERNODE) && excl_level!=0) {
		bnode=node;
		/* If the bnode is near at least with one gnode included in the excl_gid of
		 * excl_level we can continue to forward the pkt*/
		for(i=0; i<bnode->links; i++) {
			level=extmap_find_level(me.ext_map, bnode->r_node[i].r_node, me.cur_quadg.levels);
			gid=bnode->r_node[i].r_node;
			if(level < excl_level || ((level == excl_level) && (gid==excl_gid)))
				return 0;
		}
		return 1;
	}
	return 0;
}

/* Exclude the from node, the setreplied node, and the node's gnode of a higher level*/
int exclude_from_and_glevel_and_setreplied(TRACER_PKT_EXCLUDE_VARS)
{
	if(node == from || exclude_glevel(node, from, pos, excl_gid, excl_level))
		return 1;
	return 0;
}
int exclude_from_and_glevel_and_closed(TRACER_PKT_EXCLUDE_VARS)
{
	if(node == from || exclude_glevel(node, from, pos, excl_gid, excl_level))
		return 1;
	node->flags|=QSPN_REPLIED;
	return 0;
}
int exclude_from_and_glevel(TRACER_PKT_EXCLUDE_VARS)
{
	if((node->flags & QSPN_CLOSED) || node == from || exclude_glevel(node, from, pos, excl_gid, excl_level))
		return 1;
	return 0;
}

/* tracer_pkt_recv: It receive a TRACER_PKT or a TRACER_PKT_CONNECT, analyzes the received pkt,
 * adds the new entry in it and forward the pkt to all the r_nodes.
 */
int tracer_pkt_recv(PACKET rpkt)
{
	brdcast_hdr  *bcast_hdr;
	tracer_hdr   *tracer_hdr;
	tracer_chunk *tracer;
	bnode_hdr    *bhdr=0;
	map_node *from, *tracer_starter;
	map_gnode *gfrom, *gnode;
	ssize_t err;
	int ret=0, ret_err;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	int gid;
	u_char level, orig_lvl;
	char *ntop, *old_bblock;
	void *void_map;

	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &tracer_hdr, &tracer, &bhdr, &bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "tracer_pkt_recv(): The %s sent an invalid tracer_pkt here.", ntop);
		xfree(ntop);
		return -1;
	}

	hops=tracer_hdr.hops;
	gid=orig_lvl=bcast_hdr->g_node;
	level=bcast_hdr->level;
	if(!level || level == 1) {
		level=0;
		from=node_from_pos(tracer[hops-1].node, me.int_map);
		tracer_starter=node_from_pos(tracer[0].node, me.int_map);
		void_map=me.int_map;
	} else {
		level--;
		from=gfrom=gnode_from_pos(tracer[hops-1].node, ext_map[_EL(level)]);
		tracer_starter=gnode_from_pos(tracer[0].node, ext_map[_EL(level)]);
		void_map=me.ext_map;
	}

	/*This is the check for the broadcast id. If it is <= tracer_starter->brdcast
	 * the pkt is an old broadcast that still dance around.
	 */
	if(rpkt.hdr.id <= tracer_starter->brdcast) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "tracer_pkt_recv(): Received from %s an old tracer_pkt broadcast: %d", ntop, rpkt.hdr.id);
		xfree(ntop);
		return -1;
	} else
		tracer_starter->broadcast=rpkt.hdr.id;


	/*Time to update our map*/
	if(rpkt.hdr.op == TRACER_PKT) /*This check is made because tracer_pkt_recv handles also TRACER_PKT_CONNECT pkts*/
		tracer_store_pkt(void_map, level, tracer_hdr, tracer, (void *)bhdr, bblock_sz,
				&old_bchunks, &old_bblock, &old_bblock_sz);
	/*The forge of the packet.*/
	tracer_pkt_build(rpkt.hdr.op, rpkt.hdr.id, bcast_hdr->sub_id, /*IDs*/
			 gid,         orig_lvl,			      /*GnodeID and level (ignored)*/
			 bcast_hdr, tracer_hdr, tracer, 	      /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz,      /*bnode_block*/
			 &pkt);					      /*Where the pkt is built*/
	xfree(old_bblock);
	/*... forward the tracer_pkt to our r_nodes*/
	tracer_pkt_send(exclude_from_and_glevel, gid, orig_lvl, from, pkt);
	return 0;
}

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
		root_node_pos=pos_from_gnode(me.cur_quadg.gnode[_EL(level)], me.ext_map[_EL(level)]);

	me.cur_node->brdcast++;
	tracer_pkt_build(TRACER_PKT, me.cur_node->brdcast, root_node_pos,/*IDs*/
			 me.cur_quadg.gid[level],	   level,	 /*GnodeID and level*/
			 0,          0,                    0, 		 /*Received tracer_pkt*/
			 0,          0,                    0, 		 /*bnode_block*/
			 &pkt);						 /*Where the pkt is built*/
	xfree(old_bblock);
	/*Diffuse the packet in all the universe!*/
	tracer_pkt_send(exclude_from_and_glevel, me.cur_quadg.gid[level], level+1, from, pkt);
	tracer_pkt_start_mutex=0;
	return 0;
}

int tracer_pkt_connect(PACKET pkt, map_node *dst)
{
	PACKET pkt;
	map_node *from=me.cur_node;
	brdcast_hdr bcast_hdr;
	tracer_hdr tracer_hdr;
	tracer_chunk tracer[2];
	u_char gid=me.cur_quadg.gid[1], level=0; /*The tracer_pkt_connect is valid only in our gnode*/
	int root_node_pos;

	tracer_hdr.hops=2;
	tracer_hdr.bblocks=0;
	memset(&tracer[0], 0, sizeof(tracer_chunk));
	memset(&tracer[1], 0, sizeof(tracer_chunk));
	tracer[0].node=pos_from_node(me.cur_node, me.int_map);
	tracer[1].node=pos_from_node(dst, me.int_map);
	memset(&bcast_hdr, 0, sizeof(brdcast_hdr));
	bcast_hdr.gnode=me.cur_gid;
	bcast_hdr.gttl=1;
	bcast_hdr.sz=sizeof(tracer_hdr)+(sizeof(tracer_chunk)*2);
	
	me.cur_node->brdcast++;
	root_node_pos=pos_from_node(me.cur_node, me.int_map);
	tracer_pkt_build(TRACER_PKT_CONNECT, me.cur_node->brdcast, root_node_pos,/*IDs*/
			 gid,		     level,
			 &bcast_hdr,         &tracer_hdr,          tracer,      /*Received tracer_pkt*/
			 0,                  0,              0, 		/*bnode_block*/
			 &pkt);				 			/*Where the pkt is built*/
	xfree(old_bblock);
	/*Diffuse the packet in all the universe!*/
	tracer_pkt_send(exclude_from_and_glevel, gid, level, from, pkt);
	return 0;
}
