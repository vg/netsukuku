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
 * is non 0, it checks if it is equal to the last entry in the packet.
 */
int tracer_verify_pkt(tracer_chunk *tracer, u_int hops, map_node *real_from)
{
	map_node *from;
	int e, x=0;

	/*"from" has to be absolutely one of our rnodes*/
	from=node_from_pos(tracer[hops-1].node, me.int_map);
	if(from && from != real_from) {
		return -1;
	}
	for(e=0; e<me.cur_node->links; e++)
		if((int)*me.cur_node->r_node[e].r_node == (int)*from) {
			x=1;
			break;
		}
	if(!x) 
		return -1;

	return 0;
}

/* tracer_add_entry: Add our entry `node' to the tracer pkt `tracer' wich has `hops'.
 * It returns the modified tracer pkt in a newly mallocated struct and it increments the `*hops'.
 * If `tracer' is null it will return the new tracer_pkt.
 * On errors it returns NULL.*/
tracer_chunk *tracer_add_entry(map_node *map, map_node *node, tracer_chunk *tracer, u_int *hops)
{
	tracer_chunk *t;

	*hops++;
	t=xmalloc(sizeof(tracer_chunk)*hops);
	if(tracer || *hops)
		memcpy(t, tracer, sizeof(tracer_chunk) * (*hops-1));
	t[hops-1].node=pos_from_node(node, map);
	memcpy(&t[hops-1].rtt, &me.cur_node->r_node[e].rtt, sizeof(struct timeval));

	return t;
}

/* tracer_build_bentry: It builds the bnode_block to be added in the bnode's entry in 
 * the tracer pkt. It stores in `bnode_chunk' the pointer to the first bnode_chunk and
 * returns a pointer to the bnode_hdr. On errors it returns a NULL pointer.*/
bnode_hdr *tracer_build_bentry(map_node *map, map_node *node, bnode_chunk *bnode_chunk) 
{
	bnode_hdr *bhdr;
	bnode_chunk *bchunk;
	int i, bm;
	
	if(!(node->flags & MAP_BNODE))
		goto error;
	
	if((bm=map_find_bnode(me.bnode_map, me.bmap_nodes, node))==-1) 
		goto error;

	/*This will never happen, but we know the universe is fucking bastard*/
	if(!me.bnode_map[bm].links)
		goto error;

	bhdr=xmalloc(sizeof(bnode_hdr));
	bchunk=xmalloc(sizeof(bnode_chunk)*me.bnode_map[bm].links);
	memset(bhdr, 0, sizeof(bnode_hdr));
	memset(bchunk, 0, sizeof(bnode_chunk)*me.bnode_map[bm].links);

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
 * It returns 0 if the packet is valid, otherwise -1 is returned.
 */
int tracer_unpack_pkt(PACKET rpkt, brdcast_hdr *new_bcast_hdr, tracer_hdr *new_tracer_hdr, tracer_chunk *new_tracer, 
		      bnode_hdr *new_bhdr, size_t *new_bblock_sz)
{
	brdcast_hdr *bcast_hdr=rpkt.msg;
	tracer_hdr  *tracer_hdr=rpkt.msg+sizeof(brdcast_hdr);
	tracer_chunk *tracer=rpkt.msg+sizeof(brdcast_hdr)+sizeof(tracer_hdr);
	bnode_hdr    *bhdr=0;
	map_node *real_from;
	size_t bblock_sz=0, tracer_sz=0;

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
	if(bcast_hdr->g_node != me.cur_gid  || !(rpkt.hdr.flags & BCAST_PKT) ||
			!(bcast_hdr->flags & BCAST_TRACER_PKT))
		return -1;

	iptomap(me.int_map, rpkt.from, me.ipstart, &real_from);
	if(tracer_verify_pkt(tracer, tracer_hdr->hops, real_from))
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
 * after using tracer_split_bblock.
 */
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

/* tracer_store_pkt: This is the main function used to keep the map's karma in peace.
 * It updates the `map' with the given `tracer' pkt. The bnode blocks (if any) are
 * unpacked and used to update the data of the boarding gnodes.
 * In `*bblocks_found' it stores the number of bblocks considered and stores in 
 * `bblocks_found_block' these bblocks. The `bblocks_found_block' will be appended
 * in the new tracer_pkt, after our bblock entry (if any). 
 * Remember to xfree(bblocks_found_block);
 * 
 * PS: I hate myself. How can I use so many args? Damn!
 */
int tracer_store_pkt(map_node *map, tracer_hdr *tracer_hdr, tracer_chunk *tracer, 
		     void *bnode_block_start, size_t bblock_sz, 
		     u_short *bblocks_found,  char *bblocks_found_block, size_t *bblock_found_sz)
{
	bnode_hdr 	*bblock_hdr;
	bnode_chunk 	*bblock;	
	bnode_hdr 	**bblist_hdr=0;
	bnode_chunk 	***bblist=0;
	struct timeval trtt;
	map_node *from, *node;
	map_rnode rn;
	int i, e, diff, bm, x;
	u_int hops=tracer_hdr->hops;
	u_short bb;
	size_t found_block_sz;
	char *found_block;

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
				node=node_from_pos(bblist_hdr[i]->bnode, map);
				node->flags|=MAP_BNODE;

				/*Let's check if we have this bnode in the bmap, if not let's add it*/
				if((bm=map_find_bnode(me.bnode_map, me.bmap_nodes, node))==-1) {
					bm=me.bmap_nodes-1;
					me.bmap_nodes++;
					if(me.bmap_nodes == 1)
						me.bnode_map=xmalloc(sizeof(map_bnode));
					else
						me.bnode_map=xrealloc(me.bnode_map, sizeof(map_bnode) * me.bmap_nodes);
					me.bnode_map[bm].bnode_ptr=pos_from_node(node, map);
					me.bnode_map[bm].links=bblist_hdr[i]->links;
				}

				/*Now, we delete all the bnode's gnode_rnodes, then we store the new ones.
				 * yea, I know I'm raw, ahahaha.*/
				rnode_destroy(me.bnode_map[bm]);
				
				for(e=0; e<bblist_hdr[i]->links; e++) {
					memset(&rn, 0, sizeof(map_rnode));
					rn.r_node=GI2GMAP(me.ext_map, bblist[i][e]->gnode);
					memcpy(&rn.rtt, &bblist[i][e]->rtt, sizeof(struct timeval));
					rnode_add(&me.bnode_map[bm], &rn);

					memcpy(found_block+x, bblist_hdr[i][e], sizeof(bnode_chunk));
					x+=sizeof(bnode_chunk);
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
		if(!(node->flags & MAP_VOID)) { /*Ehi, we hadn't this node in the map. Add it.*/
			from->flags&=~MAP_VOID;
			from->flags|=MAP_UPDATE;
			me.cur_gnode->seeds++;
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
			if(node->links > MAXROUTES) { /*If we have too many routes we purge the worst ones*/
				diff=node->links - MAXROUTES;
				for(x=MAXROUTES; x < node->links; x++)
					rnode_del(node, x);
			}
			krnl_update_node(node);
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
		     brdcast_hdr *bcast_hdr, tracer_hdr *tracer_hdr, tracer_chunk *tracer,  
		     u_short old_bchunks,    char *old_bblock,       size_t old_bblock_sz,  
		     PACKET *pkt)
{
	tracer_chunk *new_tracer=0;
	bnode_hdr    *new_bhdr=0;
	bnode_chunk  *new_bchunk=0;
	int new_tracer_pkt=0, ret=0;
	u_int hops;

	if(!tracer_hdr || !tracer || !bcast_hdr) {
		new_tracer_pkt=1;
		bcast_hdr=xmalloc(sizeof(brdcast_hdr));
		memset(bcast_hdr, 0, sizeof(brdcast_hdr));
		bcast_hdr.g_node=me.cur_gid; 
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

	/*Time to append our entry in the tracer_pkt*/
	new_tracer=tracer_add_entry(me.int_map, me.cur_node, tracer, &hops);
	/*If we are a bnode we have to append the bnode_block too.*/
	if(me.cur_node->flags & MAP_BNODE)
		if((new_bhdr=tracer_build_bentry(me.int_map, me.cur_node, &new_bchunk)))
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
/* is_node_excluded: is the `node' included in the `excluded_nodes' list?*/
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
int tracer_pkt_send(int(*is_node_excluded)(map_node *, int), map_node *from, PACKET pkt)
{
	inet_prefix to;
	ssize_t err;
	char *ntop;
	int i, e=0;

	/*Forward the pkt to all our r_nodes (excluding the excluded hehe;)*/
	for(i=0; i<me.cur_node->links; i++) {
		if(is_node_excluded((map_node *)me.cur_node->r_node[i].r_node, from, i))
			continue;

		memset(&to, 0, sizeof(inet_prefix));
		maptoip(*me.int_map, *me.cur_node->r_node[i].r_node, me.ipstart, &to);
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
int exclude_from_and_gnode(map_node *node, map_node *from, int pos) 
{
	if(node == from || (node->flags & MAP_GNODE))
		return 1;
	return 0;
}
int exclude_from_and_gnode_and_setreplied(map_node *node, map_node *from, int pos) 
{
	if(node == from || (node->flags & MAP_GNODE))
		return 1;
	node->flags|=QSPN_REPLIED;
	return 0;
}
int exclude_from_and_gnode_and_closed(map_node *node, map_node *from, int pos) 
{
	if((node->flags & QSPN_CLOSED) || node == from || (node->flags & MAP_GNODE))
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
	ssize_t err;
	int ret=0, ret_err;
	u_int hops;
	size_t bblock_sz=0, old_bblock_sz;
	u_short old_bchunks=0;
	char *ntop, *old_bblock;

	ret_err=tracer_unpack_pkt(rpkt, &bcast_hdr, &tracer_hdr, &tracer, &bhdr, &bblock_sz);
	if(ret_err) {
		ntop=inet_to_str(&rpkt->from);
		debug(DBG_NOISE, "tracer_pkt_recv(): The %s sent an invalid tracer_pkt here.", ntop);
		xfree(ntop);
		return -1;
	}

	hops=tracer_hdr.hops;
	from=node_from_pos(tracer[hops-1].node, me.int_map);
	tracer_starter=node_from_pos(tracer[0].node, me.int_map);

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
	if(rpkt.hdr.op == TRACER_PKT) /*This is because tracer_pkt_recv handles also TRACER_PKT_CONNECT pkts*/
		tracer_store_pkt(me.int_map, tracer_hdr, tracer, (void *)bhdr, bblock_sz,
				&old_bchunks, &old_bblock, &old_bblock_sz);
	/*The forge of the packet.*/
	tracer_pkt_build(rpkt.hdr.op, rpkt.hdr.id, bcast_hdr->sub_id,  			   /*IDs*/
			 bcast_hdr, tracer_hdr, tracer, 				   /*Received tracer_pkt*/
			 old_bchunks, old_bblock, old_bblock_sz, 			   /*bnode_block*/
			 &pkt);								   /*Where the pkt is built*/
	xfree(old_bblock);
	/*... forward the tracer_pkt to our r_nodes*/
	tracer_pkt_send(exclude_from_and_gnode, from, pkt);
	return 0;
}

int tracer_pkt_start(void)
{
	PACKET pkt;
	map_node *from=me.cur_node;
	
	if(tracer_pkt_start_mutex)
		return 0;
	else
		tracer_pkt_start_mutex=1;

	me.cur_node->brdcast++;
	tracer_pkt_build(TRACER_PKT, me.cur_node->brdcast, pos_from_node(me.cur_node, me.int_map),/*IDs*/
			 0,          0,              0, 				    /*Received tracer_pkt*/
			 0,          0,              0, 			  	    /*bnode_block*/
			 &pkt);								    /*Where the pkt is built*/
	xfree(old_bblock);
	/*Diffuse the packet in all the universe!*/
	tracer_pkt_send(exclude_from_and_gnode, from, pkt);
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
	tracer_pkt_build(TRACER_PKT_CONNECT, me.cur_node->brdcast, pos_from_node(me.cur_node, me.int_map),/*IDs*/
		         &bcast_hdr,         &tracer_hdr,          tracer,		    /*Received tracer_pkt*/
			 0,                  0,              0, 			    /*bnode_block*/
			 &pkt);				 				    /*Where the pkt is built*/
	xfree(old_bblock);
	/*Diffuse the packet in all the universe!*/
	tracer_pkt_send(exclude_from_and_gnode, from, pkt);
	return 0;
}
