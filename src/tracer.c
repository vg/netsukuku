#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>

#include "tracer.h"
#include "misc.h"
#include "log.h"
#include "xmalloc.h"

extern int my_family;
extern struct current me;

int tracer_verify_pkt(struct tracer_chunk *tracer, int hops)
{
	map_node *from;
	int e, x=0;

	/*"from" has to be absolutely one of our rnodes*/
	from=node_from_pos(tracer[hops-1].node, me.int_map);
	for(e=0; e<me.cur_node->links; e++)
		if((int)*me.cur_node->r_node[e].r_node == (int)*from) {
			x=1;
			break;
		}
	if(!x) {
		error("The tracer pkt is invalid! We don't have %d as rnode", t[hops-1].node);
		return -1;
	}

	return 0;
}

/* tracer_add_entry: Add our entry ("node") to the tracer pkt `tracer' wich has `hops'.
 * It returns the modified tracer pkt in a newly mallocated struct and it increments the `*hops' */
struct tracer_chunk *tracer_add_entry(map_node *map, map_node *node, struct tracer_chunk *tracer, int *hops)
{
	struct tracer_chunk *t;

	if(tracer_verify_pkt(tracer, *hops))
		return 0;
	*hops++;
	t=xmalloc(sizeof(struct tracer_chunk)*hops);
	memcpy(t, tracer, sizeof(struct tracer_chunk) * (*hops-1));
	t[hops-1].node=pos_from_node(node, map);
	memcpy(&t[hops-1].rtt, &me.cur_node->r_node[e].rtt, sizeof(struct timeval));

	return t;
}

/* tracer_build_bentry: It builds the bnode_block to be added in the bnode's entry in 
 * the tracer pkt. It stores in `bnode_chunk' the pointer to the first bnode_chunk and
 * returns a pointer to the bnode_hdr. On errors it returns a NULL pointer.*/
struct bnode_hdr *tracer_build_bentry(map_node *map, map_node *node, struct bnode_chunk *bnode_chunk) 
{
	struct bnode_hdr *bhdr;
	struct bnode_chunk *bchunk;
	int i, bm;
	
	if(!(node->flags & MAP_BNODE))
		goto error;
	
	if((bm=map_find_bnode(me.bnode_map, me.bmap_nodes, node))==-1) 
		goto error;

	/*This will never happen, but we know the universe is fucking bastard*/
	if(!me.bnode_map[bm].links)
		goto error;

	bhdr=xmalloc(sizeof(struct bnode_hdr));
	bchunk=xmalloc(sizeof(struct bnode_chunk)*me.bnode_map[bm].links);
	memset(bhdr, 0, sizeof(struct bnode_hdr));
	memset(bchunk, 0, sizeof(struct bnode_chunk)*me.bnode_map[bm].links);

	bhdr->bnode=pos_from_node(node, map);
	for(i=0; i<me.bnode_map[bm].links; i++) {
		bchunk[i].gnode=GMAP2GI(me.ext_map, me.bnode_map[bm].r_node[i].r_node);
		memcpy(&bchunk[i].rtt, &me.bnode_map[bm].r_node[i].rtt, sizeof(struct timeval));
		bhdr->links++;
	}

	*bnode_chunk=bchunk;
	return bhdr;
error:
	*bnode_chunk=0;
	return 0;
}

/* tracer_pack_pkt: do ya need explanation? pretty simple: pack the tracer packet*/
char *tracer_pack_pkt(struct bcast_hdr *bcast_hdr, struct tracer_hdr *tracer_hdr, struct tracer_chunk *tracer, 
		      struct bnode_hdr *bhdr, struct bnode_chunk *bchunk)
{
	size_t bblock_sz=0, pkt_sz;
	char *msg, *buf;

	if(bhdr && bchunk)
		bblock_sz=BNODEBLOCK_SZ(bhdr->links);

	pkt_sz=BRDCAST_SZ(TRACERPKT_SZ(hops))+bblock_sz;
	
	buf=msg=xmalloc(pkt_sz);
	memset(msg, 0, pkt_sz);

	memcpy(buf, bcast_hdr, sizeof(struct brdcast_hdr));
	buf+=sizeof(struct brdcast_hdr);
	memcpy(buf, tracer_hdr, sizeof(struct tracer_hdr));
	buf+=sizeof(struct tracer_hdr);
	memcpy(buf, tracer, sizeof(struct tracer_chunk)*tracer_hdr->hops);
	if(bhdr && bchunk) {
		buf+=sizeof(struct tracer_chunk)*tracer_hdr->hops;
		memcpy(buf, bhdr, sizeof(struct bnode_hdr));
		buf+=sizeof(struct bnode_hdr);
		memcpy(buf, bchunk, sizeof(struct bnode_chunk));
	}

	return msg;
}

/* tracer_split_bblock: It searches from bnode_block_start to bnode_block_start+bblock_sz for bnode block.
 * It puts the address of the found bblock_hdr in the `bbl_hdr' (bnode block list) and the address
 * pointing to the start of the bnode_chunk in the `bbl'.
 * It then returns the number of bblock found. Remember to xfree bbl_hdr and bbl 
 * after using tracer_split_bblock.
 */
int tracer_split_bblock(void *bnode_block_start, size_t bblock_sz, struct bnode_hdr *bbl_hdr, struct bnode_chunk *bbl)
{
	struct bnode_hdr 	*bblock_hdr;
	struct bnode_chunk 	*bblock;
	struct bnode_hdr 	**bblist_hdr=0;
	struct bnode_chunk 	***bblist=0;
	int e,p,x=0;
		
	if(!bblock_sz)
		return -1;
	
	for(e=0; e < bblock_sz; ) {
		bblock_hdr=bnode_block_start+e;
		bblock=(void *)bblock_hdr+sizeof(struct bnode_hdr);

		if(bblock_hdr->links <= 0)
			continue;

		/*Are we going far away the end of the buffer?*/
		if(bblock_sz-e < sizeof(struct bnode_hdr)+(sizeof(struct bnode_chunk)*bblock_hdr->links))
			break;
		
		bblist_hdr=xrealloc(bblist, sizeof(struct bnode_hdr *) * (x+1));
		bblist=xrealloc(bblist, sizeof(struct bnode_chunk *) * (x+1));

		bblist_hdr[x]=bblock_hdr;
		bblist[x]=xmalloc(sizeof(struct bnode_chunk *) * bblock_hdr->links);
		for(p=0; p<bblock_hdr->links; p++)
			bblist[x][p]=(void *)bblock+(sizeof(struct bnode_chunk)*p);
		
		x++;
		e+=BNODEBLOCK_SZ(bblock_hdr->links);
	}
	
	*bbl_hdr=bblist_hdr;
	*bbl=bblist;
	return x;
}

/* tracer_store_pkt: This is the main function used to keep the map's karma in peace.
 * It updates the `map' with the given `tracer' pkt. The bnode blocks (if any) are
 * unpacked and used to update the data of the boarding gnodes.
 */
int tracer_store_pkt(map_node *map, struct tracer_hdr *tracer_hdr, struct tracer_chunk *tracer, 
		     u_short hops, void *bnode_block_start, size_t bblock_sz)
{
	struct bnode_hdr 	*bblock_hdr;
	struct bnode_chunk 	*bblock;	
	struct bnode_hdr 	**bblist_hdr=0;
	struct bnode_chunk 	***bblist=0;
	struct timeval trtt;
	map_node *from, *node;
	map_rnode rn;
	int i, e, diff, bb, bm;

	if(tracer_verify_pkt(tracer, *hops))
		return 0;

	from=node_from_pos(tracer[hops-1].node, map);
	if(!(from.flags & MAP_RNODE)) { /*the `from' node isn't in our r_nodes. Add it!*/
		map_rnode rnn;

		memset(&rnn, '\0', sizeof(map_rnode));
		rnn.r_node=from;
		memcpy(&rnn.rtt, &tracer[hops-1].rtt, sizeof(struct timeval));
		rnode_add(root_node, &rnn);

		from->flags|=MAP_RNODE;
		from->flags&=~MAP_VOID & ~MAP_UPDATE;
	}

	if(bblock_sz) {	/*Well, well, we have to take care of bnode blocks*/
		bb=tracer_split_bblock(bnode_block_start, bblock_sz, &bbl_hdr, &bbl);
		if(!bb || bb!=tracer_hdr->bblocks)
			error("tracer_store_pkt(): malformed bnode block.");
		else {
			for(i=0; i<bb; i++) {
				if(bblist_hdr[i]->bnode >= MAXGROUPNODE || bblist_hdr[i]->bnode < 0)
					continue;
				node=node_from_pos(bblist_hdr[i]->bnode, map);
				node->flags|=MAP_BNODE;

				/*Let's check if we have this bnode in the bmap, if not let's add it*/
				if((bm=map_find_bnode(me.bnode_map, me.bmap_nodes, node))==-1) {
					bm=me.bmap_nodes-1;
					me.bmap_nodes++;
					me.bnode_map=xrealloc(me.bnode_map, sizeof(map_bnode *) * me.bmap_nodes);
					me.bnode_map[bm].bnode_ptr=pos_from_node(node, map);
					me.bnode_map[bm].links=bblist_hdr[i]->links;
				}

				/*Now, we delete all the bnode's gnode_rnodes, then we store the new ones.
				 * yea, I know I'm raw, ahahaha.*/
				rnode_destroy(me.bnode_map[bm]);
				
				for(e=0; e<bblist_hdr[i]->links; e++) {
					memset(&rn, 0, sizeof(map_rnode));
					rn.r_node=GI2GMAP(me.ext_map, bblist[i][e].gnode);
					memcpy(&rn.rtt, &bblist[i][e].rtt, sizeof(struct timeval));
					rnode_add(&me.bnode_map[bm], &rn);
				}

				xfree(bblist[i]);
			}
			xfree(bblist_hdr);
			xfree(bblist);
		}
	}
		
	timeradd(tracer[0].rtt, &trtt, &trtt);
	for(i=hops-1; i != hops; i--) {
		timeradd(tracer[i].rtt, &trtt, &trtt);

		node=node_from_pos(tracer[i].node, map);
		if(!(node->flags & MAP_VOID)) {
			from->flags&=~MAP_VOID;
			from->flags|=MAP_UPDATE;
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
			memcpy(&rnn.rtt, &trtt, sizeof(struct timeval));
			rnode_add(node, &rnn);

			node->flags|=MAP_UPDATE;
		}

		/*ok, now the kernel needs a refresh*/
		if(node->flags & MAP_UPDATE) {
			rnode_trtt_order(node);
			krnl_update_node(node);
		}
	}
	return 0;
}

/* tracer_pkt_send: Normal tracer_pkt being sent*/
int tracer_pkt_send(PACKET pkt)
{
	/*TODO*/
}
