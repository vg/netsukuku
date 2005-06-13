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
 */

#include "includes.h"

#include "llist.c"
#include "ipv6-gmp.h"
#include "inet.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"


/* 
 * get_groups: It returns how many groups there are in the given level.
 */
inline int get_groups(int family, int lvl)
{ 				
	if( lvl == GET_LEVELS(family))
		return 1;
	else
		return MAXGROUPNODE;
}

/*pos_from_gnode: Position from gnode: It returns the position of the `gnode' in the `map'.*/
int pos_from_gnode(map_gnode *gnode, map_gnode *map)
{
	return (int)(((char *)gnode-(char *)map)/sizeof(map_gnode));
}

/*Gnode from position: it returns the fnode pointer calculated by the given `pos' in the map*/
map_gnode *gnode_from_pos(int pos, map_gnode *map)
{
	return (map_gnode *)((pos*sizeof(map_gnode))+(char *)map);
}

/* 
 * rnodetoip: converts the node `maprnode', which is a rnode of the root_node,
 * to the relative ip.
 */
void rnodetoip(u_int mapstart, u_int maprnode, inet_prefix ipstart, 
		inet_prefix *ret)
{
	ext_rnode *e_rnode;
	map_node *rnode=(map_node *)maprnode;

	memset(ret, 0, sizeof(inet_prefix));
	if(rnode->flags & MAP_ERNODE) {
		e_rnode=(ext_rnode *)rnode;
		memcpy(ret, &e_rnode->quadg.ipstart[0], sizeof(inet_prefix));
	} else 
		maptoip(mapstart, maprnode, ipstart, ret);
}

void maxgroupnode_level_init(void)
{
	mpz_t base;
	int i, e;
	mpz_init(base);	
	
	mpz_set_ui(base, MAXGROUPNODE);
	for(i=0, e=0; i<MAX_LEVELS+1; i++, e++) {
		mpz_init(maxgroupnode_levels[i]);
		mpz_pow_ui(maxgroupnode_levels[i], base, e);
	}
	mpz_clear(base);
}

void maxgroupnode_level_free(void)
{
	int i;
	for(i=0; i<MAX_LEVELS+1; i++) 
		mpz_clear(maxgroupnode_levels[i]);
}

/* 
 * iptogid: ip to gnode id, of the specified `level', conversion function.
 * Note: this function cannot fail! So be sure to pass a valid `level'.
 */
int iptogid(inet_prefix ip, int level)
{
	mpz_t xx, yy, zz;
	int upper_level, h_ip[4];
	size_t count;

	memcpy(h_ip, ip.data, 4);
	upper_level=level+1;

	mpz_init(xx);
	mpz_init(yy);
	mpz_init(zz);

	mpz_import(yy, 4, HOST_ORDER, sizeof(int), NATIVE_ENDIAN, 0, h_ip);

	/* 
	 * gid=(ip/MAXGROUPNODE^level) - (ip/MAXGROUPNODE^(level+1) * MAXGROUPNODE);
	 */
	mpz_tdiv_q(xx, yy, maxgroupnode_levels[level]);
	mpz_tdiv_q(zz, yy, maxgroupnode_levels[upper_level]);
	mpz_mul_ui(zz, zz, MAXGROUPNODE);
	mpz_sub(yy, xx, zz);

	memset(h_ip, '\0', sizeof(int)*4);
	mpz_export(h_ip, &count, HOST_ORDER, sizeof(int), NATIVE_ENDIAN, 0, yy);
	
	mpz_clear(xx);
	mpz_clear(yy);
	mpz_clear(zz);
	
	return h_ip[0];
}

/* 
 * gidtoipstart: It sets in `*ip' the ipstart of the gnode using the `gid[x]' for
 * each level x.
 * `total_levels' is the total number of levels and the `gid' array 
 * has `total_levels' elements.
 * `levels' is the number of array elements considered, gidtoipstart() will use
 * only the elements going from gid[total_levels-levels] to gid[total_levels].
 * `family' is used to fill the inet_prefix of ipstart.
 */
void gidtoipstart(int *gid, u_char total_levels, u_char levels, int family, 
		inet_prefix *ip)
{
	mpz_t xx, yy;
	int i, h_ip[4];
	size_t count;

	memset(h_ip, '\0', sizeof(h_ip[0])*4);
	mpz_init(xx);
	mpz_init(yy);
	mpz_set_ui(yy, 0);
	mpz_set_ui(xx, 0);

	for(i=total_levels-ZERO_LEVEL; i>=total_levels-levels; i--) {
		/* ipstart += MAXGROUPNODE^i * gid[i]; */
		mpz_mul_ui(xx, maxgroupnode_levels[i], gid[i]);
		mpz_add(yy, yy, xx);
	}
	
	mpz_export(h_ip, &count, HOST_ORDER, sizeof(h_ip[0]), NATIVE_ENDIAN, 0, yy);
	mpz_clear(xx);
	mpz_clear(yy);

	memcpy(ip->data, h_ip, sizeof(int)*4);
	ip->family=family;
	ip->len = (family == AF_INET) ? 4 : 16;
	ip->bits=ip->len*8;
}

/* 
 * iptoquadg: Using the given `ip' it fills the `qg' quadro_group struct. The `flags'
 * given specify what element fill in the struct (the flags are in gmap.h).
 */
void iptoquadg(inet_prefix ip, map_gnode **ext_map, quadro_group *qg, char flags)
{
	int i;
	u_char levels;
	int gid[MAX_LEVELS];

	memset(qg, 0, sizeof(quadro_group));
	
	levels=GET_LEVELS(ip.family);
	qg->levels=levels;

	if(flags & QUADG_GID) {
		for(i=0; i<levels; i++) {
			qg->gid[i]=iptogid(ip, i);
			gid[i]=qg->gid[i];
		}
		
		if(flags & QUADG_IPSTART)
			for(i=0; i<levels; i++)
				gidtoipstart(gid, levels, levels-i, ip.family,
						&qg->ipstart[i]);
	}
	if(flags & QUADG_GNODE) {
		for(i=0; i<levels-ZERO_LEVEL; i++)
			qg->gnode[i]=gnode_from_pos(qg->gid[i+1], ext_map[i]);
		qg->gnode[levels-ZERO_LEVEL]=&ext_map[i][0];
	}
}

void quadg_free(quadro_group *qg)
{
	memset(qg, 0, sizeof(quadro_group));
}

void quadg_destroy(quadro_group *qg)
{
	quadg_free(qg);
	xfree(qg);
}

/*
 * random_ip: It generates a new random ip. 
 * If `ipstart' is not NULL the new ip is restricted in the `final_gid' of
 * `final_level', so it'll be taken inside this range:
 * 	A=ipstart + (MAXGROUPNODE^( final_level + 1)) * final_gid;
 * 	B=ipstart + (MAXGROUPNODE^( final_level + 1)) * (final_gid + 1);
 * 		A <= x <= B
 * If `ipstart' is NULL a completely random ip is generated.
 * `total_levels' is the maximum number of levels.
 * `ext_map' is an external map.
 * If `only_free_gnode' is not 0, only the available and empty gnode are chosen.
 * The new ip is stored in `new_ip'.
 * */
void random_ip(inet_prefix *ipstart, int final_level, int final_gid, 
		int total_levels, map_gnode **ext_map, int only_free_gnode, 
		inet_prefix *new_ip, int my_family)
{
	int i, level, levels;
	int gid[total_levels];
	quadro_group qg;

	memset(new_ip, 0, sizeof(inet_prefix));
	
	if(!ipstart || final_level==total_levels) {
		u_int idata[4]={0,0,0,0};
		
		/* 
		 * Let's choose a completely random ip. We must ensure that it
		 * is completely unique, because if two gnode are created with
		 * the same ip it's a mess. To solve this problem we use the
		 * hash_time function.
		 */
		levels=total_levels;
		if(my_family == AF_INET)
			idata[0]=hash_time(0,0);
		else {
			hash_time((int *)&idata[0], (int *)&idata[1]);
			hash_time((int *)&idata[2], (int *)&idata[3]);
		}
		
		inet_setip(new_ip, idata, my_family);
		
		return;
	} else {
		/*
		 * We can choose only a random ip which is inside the final_gid.
		 * The `final_gid' is a gnode of the `final_level' level.
		 * The `final_level' has its `ipstart'; with it we determine
		 * the its higher levels.
		 * The work is done in this way:
		 * - ipstart is splitted in gnode_ids and they are placed in qg.gid.
		 * - The final_level-1 gid is set to final_gid.
		 * - The gids of levels lower than `final_level' are chosen
		 *   randomly.
		 * - The gids of levels higher than `final_level' are set to the
		 *   gids of qg.gid[x >= final_level].
		 * - The ipstart is recomposed from the gids.
		 */
		levels=final_level-1;
		iptoquadg(*ipstart, ext_map, &qg, QUADG_GID);
		gid[final_level-1]=final_gid;
		for(i=final_level; i<total_levels; i++)
			gid[i]=qg.gid[i];
	}

	/*
	 * Now we choose random gids for each level so we'll have a random ip
	 * with gidtoipstart();
	 */
	for(level=0; level < levels; level++) {
		gid[level]=rand_range(0, get_groups(my_family, level)-1);
		if(level && only_free_gnode) {
			/* 
			 * We have to be sure that we're not picking a gnode 
			 * already used in the ext_map. Generally when we hook
			 * we have loaded the old ext_map, so skipping the
			 * taken gnodes we increase the possibility to create a
			 * brand new, and not already used, gnode.
			 */
			while(!(ext_map[_EL(level)][gid[level]].flags & MAP_VOID))
				gid[level]=rand_range(0, get_groups(my_family, level)-1);
		}
	}

	/* 
	 * Ok, we've set the gids of each level so we recompose them in the
	 * new_ip.
	 */
	gidtoipstart(gid, total_levels, total_levels, my_family, new_ip);
}

/*
 * gnodetoip: It converts the gnode which has the given `gnodeid' at `level'
 * to its corresponding ipstart. The `quadg' struct must refer to the given
 * gnode.
 * The ip is stored in `ip', and the ip->bits are choosen carefully in the 
 * CIDR blocks format, in this way the `ip' includes also the ranges of the 
 * gnode's level: ip <= x <= ip+MAXGROUPNODE^(level+1).
 */
void gnodetoip(quadro_group *quadg, int gnodeid, u_char level, inet_prefix *ip)
{
	int gid[quadg->levels];
	
	if(level > quadg->levels || !level)
		return;
	
	memcpy(gid, quadg->gid, sizeof(int)*quadg->levels);
	gid[level]=gnodeid;
	
	gidtoipstart(gid, quadg->levels, quadg->levels-level, 
			quadg->ipstart[0].family, ip);

	ip->bits-=(level*MAXGROUPNODE_BITS);
}

/* 
 * quadg_gids_cmp: Compares the gids of `a' and `b' starting from the `lvl'th 
 * level. If the gids compared are the same, zero is returned.
 */
int quadg_gids_cmp(quadro_group a, quadro_group b, int lvl)
{
	int i;
	
	if(a.levels != b.levels)
		return 1;

	for(i=lvl; i < a.levels; i++)
		if(a.gid[i] != b.gid[i])
			return 1;
	return 0;
}

/* 
 * * * External rnodes functions * * *
 */

/* e_rnode_init: Initialize an ext_rnode_cache list and zeros the `counter' */
ext_rnode_cache *e_rnode_init(u_int *counter)
{
	ext_rnode_cache *erc;

	/*list_init(erc, 0);*/
	erc=0;
	if(counter)
		*counter=0;
	return erc;
}

/* e_rnode_free: destroy an ext_rnode_cache list */
void e_rnode_free(ext_rnode_cache **erc, u_int *counter)
{
	if(counter)
		*counter=0;
	if(!*erc)
		return;
	list_destroy(*erc);
	*erc=0;
}

/* 
 * e_rnode_add: adds an external node in the ext_rnode_cache list.
 */
void e_rnode_add(ext_rnode_cache **erc, ext_rnode *e_rnode, int rnode_pos, u_int *counter)
{
	ext_rnode_cache *p;

	p=xmalloc(sizeof(ext_rnode_cache));
	memset(p, 0, sizeof(ext_rnode_cache));
	
	p->e=e_rnode;
	p->rnode_pos=rnode_pos;
	
	if(!(*counter) || !erc)
		list_init(*erc, p);
	else
		list_add(*erc, p);
	(*counter)++;
}

void e_rnode_del(ext_rnode_cache **erc_head, u_int *counter, ext_rnode_cache *erc)
{
	if((*counter) <= 0 || !erc)
		return;

	if(erc->e) {
		xfree(erc->e);
		erc->e=0;
	}
	
	*erc_head=list_del(*erc_head, erc);
	(*counter)--;
}

/*
 * erc_update_rnodepos: When a rnode is deleted from the root_node all the
 * erc->rnode_pos vars must be updated. For example if there's 
 * an  erc->rnode_pos == 5 and the 4th rnode is deleted, than the 5th rnode
 * doesn't exist anymore because it is swapped in the 4th position.
 * The `old_rnode_pos' holds the deleted rnode position.
 * Note: it's assumed that the old rnode has been alread deleted.
 */
void erc_update_rnodepos(ext_rnode_cache *erc, map_node *root_node, int old_rnode_pos)
{
	ext_rnode_cache *p=erc;

	if(!erc)
		return;

	/* If the old rnode was in the last position, it wasn't swapped */
	if(old_rnode_pos == root_node->links)
		return;

	list_for(p) {
		if(!p->e)
			continue;

		/* If the ext_rnode was in the last position, now it is swapped
		 * in `old_rnode_pos' */
		if(p->rnode_pos == root_node->links)
			p->rnode_pos=old_rnode_pos;
	}

	return;
}

/* 
 * erc_find: Searches in the `erc' ext_rnode_cache list a struct which has the
 * erc->e == e_rnode and returns it.
 */
ext_rnode_cache *
erc_find(ext_rnode_cache *erc, ext_rnode *e_rnode)
{
	ext_rnode_cache *p=erc;
	if(!erc)
		return 0;

	list_for(p) {
		if(!p->e)
			continue;
		if(p->e == e_rnode)
			return p;
	}
	return 0;
}

/* 
 * e_rnode_find: It searches in the `erc' list a quadro_group struct 
 * equal to `qg', by comparing their gids that goes from gid[`level'] to 
 * gid[`qg->levels'].
 * If an ext_rnode which has such struct is found it returns the pointer to the
 * struct.
 * If nothing is found 0 is returned.
 */
ext_rnode_cache *
e_rnode_find(ext_rnode_cache *erc, quadro_group *qg, int level)
{
	ext_rnode_cache *p=erc;

	if(!erc)
		return 0;

	list_for(p) {
		if(!p->e)
			continue;
		if(!quadg_gids_cmp(p->e->quadg, *qg, level))
			return p;
	}
	return 0;
}

/* 
 * erc_find_gnode; Returns the ext_rnode_cache having 
 * erc->e->quadg.gnode[_EL( `level' )] == `gnode'
 */
ext_rnode_cache *
erc_find_gnode(ext_rnode_cache *erc, map_gnode *gnode, u_char level)
{
	ext_rnode_cache *p=erc;

	if(!erc || !level)
		return 0;

	list_for(p) {
		if(!p->e)
			continue;

		if(p->e->quadg.gnode[_EL(level)] == gnode)
				return p;
	}
	
	return 0;
}

/* 
 * * * External map functions * * *
 */

map_gnode *init_gmap(int groups)
{
	map_gnode *gmap;
	size_t len;
	
	if(!groups)
		groups=MAXGROUPNODE;
	len=sizeof(map_gnode) * groups;
	gmap=(map_gnode *)xmalloc(len);
	memset(gmap, '\0', len);

	reset_gmap(gmap, groups);
	
	return gmap;
}

void reset_gmap(map_gnode *gmap, int groups)
{
	int i;
	size_t len;

	if(!groups)
		groups=MAXGROUPNODE;
	len=sizeof(map_gnode)*groups;
	
	for(i=0; i<groups; i++)
		gmap_node_del(&gmap[i]);
}

/* init_gmap: Initialize an ext_map with `levels' gmap. Each gmap
 * has `groups' elements*/
map_gnode **init_extmap(u_char levels, int groups)
{
	map_gnode **ext_map;
	int i;

	if(!levels)
		levels=MAX_LEVELS;
	if(!groups)
		groups=MAXGROUPNODE;

	ext_map=(map_gnode **)xmalloc(sizeof(map_gnode *) * (levels));
	levels--; /*We strip off the Zero level*/
	for(i=0; i<levels; i++)
		ext_map[i]=init_gmap(groups);
	
	/*Ok, now we stealthy add the unity_level which has only one gmap.*/
	ext_map[levels]=init_gmap(1);
	return ext_map;
}

/* free_extmap: Destroy the ext_map*/
void free_extmap(map_gnode **ext_map, u_char levels, int groups)
{
	int e;

	if(!levels)
		levels=MAX_LEVELS;
	if(!groups)
		groups=MAXGROUPNODE;

	levels--;
	for(e=0; e<levels; e++) {
		reset_gmap(ext_map[e], groups);
		xfree(ext_map[e]);
	}
	/*Free the unity_level map*/
	reset_gmap(ext_map[levels], 1);
	xfree(ext_map[levels]);

	xfree(ext_map);
}

void reset_extmap(map_gnode **ext_map, u_char levels, int groups)
{
	int i;
	
	for(i=1; i<levels; i++) 
		reset_gmap(ext_map[_EL(i)], groups);
	rnode_destroy(&ext_map[_EL(i)][0].g);
}

/* g_rnode_find: It searches in `gnode'.g a rnode which points to the gnode `n'.
 * It then returns the position of that rnode.
 * If the rnode is not found it returns -1;*/
int g_rnode_find(map_gnode *gnode, map_gnode *n)
{
	return rnode_find(&gnode->g, (map_node *)n);
}

/* extmap_find_level: It returns the position of the gnode map which contains 
 * the 'gnode`. This position corresponds to the level of that gmap.
 * The ext_map is given in `ext_map`. `max_level' is the maximum number of level 
 * present in the `ext_map'.
 * ex: if gnode is in ext_map[i] it will return i; 
 * On failure -1 is returned.*/
int  extmap_find_level(map_gnode **ext_map, map_gnode *gnode, u_char max_level)
{
	int i, a, b, c;
	
	for(i=1; i<max_level; i++) {
		a=(int) gnode;
		b=(int)&ext_map[_EL(i)][0];
		c=(int)&ext_map[_EL(i)][MAXGROUPNODE-1];
		
		if(a >= b && a <= c)
			return i;
	}
	return -1;
}

/* gmap_node_del: It deletes a `gnode' from the `gmap'. Really it sets the
 * gnode's flags to GMAP_VOID.*/
void gmap_node_del(map_gnode *gnode)
{
	map_node_del(&gnode->g);
	memset(gnode, 0, sizeof(map_gnode));
	gnode->flags|=GMAP_VOID;
	gnode->g.flags|=MAP_VOID;
}


/* 
 * gmap_get_rblock: It uses get_rnode_block to pack all the ext_map's rnodes
 * `maxgroupnode' is the number of nodes present in the map.
 * It returns a pointer to the start of the rnode block and stores in "count" 
 * the number of rnode structs packed.
 */
map_rnode *gmap_get_rblock(map_gnode *map, int maxgroupnode, int *count)
{
	int i, c=0, tot=0;
 	map_rnode *rblock;
	*count=0;
	
	for(i=0; i<maxgroupnode; i++)
		tot+=map[i].g.links;
	if(!tot)
		return 0;
	rblock=(map_rnode *)xmalloc(sizeof(map_rnode)*tot);

	for(i=0; i<maxgroupnode; i++)
		c+=get_rnode_block((int *)map, &map[i].g, rblock, c);

	*count=c;
	return rblock;
}

/* gmap_store_rblock: Given a correct ext_map with `maxgroupnode' elements it
 * restores all the r_node structs in the map from the rnode_block using 
 * store_rnode_block.*/
int gmap_store_rblock(map_gnode *gmap, int maxgroupnode, map_rnode *rblock)
{
	int i, c=0;

	for(i=0; i<maxgroupnode; i++)
		c+=store_rnode_block((int *)gmap, &gmap[i].g, rblock, c);
	return c;
}

/* 
 * extmap_get_rblock: It packs the rnode_block for each map present in the 
 * `ext_map'.
 * There are a total of `levels' maps in the ext_map. Each map has 
 * `maxgroupnodes' nodes. In `*ret_count' is stored an array of map's rnodes 
 * count, so each element of the array represents the number of rnodes in the 
 * rblock of the relative map.
 * It returns an array of rblock's pointer. Each array's element points to the 
 * rblock for the map in that level.
 * Ex: ret_count[n] is the number of rnodes placed in rblock[n], and n is also 
 * the level of the map which has those rnodes. I hope I was clear ^_-
 * PS: You'll have to xfree ret_count, rblock[0-levels], and rblock;
 */ 
map_rnode **extmap_get_rblock(map_gnode **ext_map, u_char levels, int maxgroupnodes, int **ret_count)
{
	int i;
 	map_rnode **rblock;
	int *count;
	
	if(!levels)
		return 0;
	rblock=xmalloc(sizeof(map_rnode *) * levels);
	count=xmalloc(sizeof(int) * levels);
	
	for(i=0; i<levels; i++) 
		rblock[i]=gmap_get_rblock(ext_map[i], maxgroupnodes, &count[i]);
	
	*ret_count=count;
	return rblock;
}

/* extmap_store_rblock: It [re]stores all the rnodes in all the maps of the `ext_map'.
 * The maps have `maxgroupnode' nodes, while the `ext_map' has `levels' levels.
 * The rnodes are taken from the `rblock'.
 * The number of map restored is returned and it shall be equal to the number of `levels'.
 */
int extmap_store_rblock(map_gnode **ext_map, u_char levels, int maxgroupnode, 
		map_rnode *rblock, size_t *rblock_sz)
{
	int i;
	for(i=0; i<levels; i++) {
		if(rblock_sz[i])
			gmap_store_rblock(ext_map[i], maxgroupnode, rblock);
		rblock = (map_rnode *)((char *)rblock + rblock_sz[i]);
	}
	return i;
}

/* verify_ext_map_hdr: What to say? It verifies the validity of an ext_map_hdr*/
int verify_ext_map_hdr(struct ext_map_hdr *emap_hdr)
{
	u_char levels;
	int maxgroupnode, i;
	
	levels=emap_hdr->quadg.levels-UNITY_LEVEL;
	maxgroupnode=emap_hdr->ext_map_sz/(sizeof(map_gnode)*levels);
	if(maxgroupnode > MAXGROUPNODE || emap_hdr->total_rblock_sz > MAXRNODEBLOCK*levels
			|| emap_hdr->ext_map_sz > maxgroupnode*sizeof(map_gnode)*levels)
		return 1;

	for(i=0; i<levels; i++)
		if(emap_hdr->quadg.gid[i] > maxgroupnode)
			return 1;

	return 0;
}

void free_extmap_rblock(map_rnode **rblock, u_char levels)
{
	int i;
	for(i=0; i<levels; i++)
		if(rblock[i])
			xfree(rblock[i]);
	xfree(rblock);
}

/* 
 * pack_extmap: It returns the packed `ext_map', ready to be saved or sent. It stores 
 * in `pack_sz' the size of the package. Each gmaps, present in the `ext_map', has 
 * `maxgroupnode' nodes. `quadg' must be a valid quadro_group struct filled with 
 * valid values. 
 */
char *pack_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, size_t *pack_sz)
{
	struct ext_map_hdr emap_hdr;
	map_gnode *packed_map;
	map_rnode **rblock;
	int *count, i;
	char *package, *p=0;
	u_char levels=quadg->levels-UNITY_LEVEL;

	/*Packing the rblocks*/
	rblock=extmap_get_rblock(ext_map, levels, maxgroupnode, &count);

	/*Building the hdr...*/
	memset(&emap_hdr, 0, sizeof(struct ext_map_hdr));
	emap_hdr.ext_map_sz=maxgroupnode*sizeof(map_gnode)*levels;
	for(i=0; i<levels; i++) {
		emap_hdr.rblock_sz[i]=count[i]*sizeof(map_rnode);
		emap_hdr.total_rblock_sz+=emap_hdr.rblock_sz[i];
	}
	memcpy(&emap_hdr.quadg, quadg, sizeof(quadro_group));
	/* emap_hdr.gnode loses its meaning */
	memset(emap_hdr.quadg.gnode, 0, sizeof(map_gnode *) * MAX_LEVELS);

	/*Let's fuse all in one*/
	*pack_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.total_rblock_sz);
	package=xmalloc(*pack_sz);
	
	memcpy(package, &emap_hdr, sizeof(struct ext_map_hdr));
	p=package+sizeof(struct ext_map_hdr);
	for(i=0; i<levels; i++) {
		memcpy(p, ext_map[i], maxgroupnode*sizeof(map_gnode));
		
		/* Remove the MAP_ME flag from the map we are packing */
		packed_map=(map_gnode *)p;
		packed_map[emap_hdr.quadg.gid[i+1]].flags&=~GMAP_ME;
		packed_map[emap_hdr.quadg.gid[i+1]].g.flags&=~MAP_ME;
		
		p+=maxgroupnode*sizeof(map_gnode);
	}
	
	if(rblock) {
		for(i=0; i<levels; i++) {
			if(!emap_hdr.rblock_sz[i])
				continue;
			memcpy(p, rblock[i], emap_hdr.rblock_sz[i]);
			p+=emap_hdr.rblock_sz[i];
		}
		free_extmap_rblock(rblock, levels);
	}

	xfree(count);
	return package;
}


/* 
 * unpack_extmap: Given a valid ext_map package (packed with pack_extmap), it 
 * allocates a brand new ext_map and restores in it the gmaps and the rnodes.
 * In `quadg' is stored the quadro_group referring to this ext_map.
 * On success the a pointer to the new ext_map is retuned, otherwise 0 will be
 * the fatal value.
 */
map_gnode **unpack_extmap(char *package, size_t pack_sz, quadro_group *quadg)
{
	map_gnode **ext_map;
	struct ext_map_hdr *emap_hdr=(struct ext_map_hdr *)package;
	map_rnode *rblock;
	u_char levels;
	int err, i, maxgroupnode;
	char *p;

	levels=emap_hdr->quadg.levels-UNITY_LEVEL;
	maxgroupnode=emap_hdr->ext_map_sz/(sizeof(map_gnode)*levels);
	
	if(verify_ext_map_hdr(emap_hdr) || 
			emap_hdr->ext_map_sz+sizeof(struct ext_map_hdr) > pack_sz) {
		error("Malformed ext_map_hdr. Aborting unpack_map().");
		return 0;
	}
	
	/*Unpacking the ext_map*/
	p=package+sizeof(struct ext_map_hdr);
	ext_map=init_extmap(emap_hdr->quadg.levels, maxgroupnode);
	for(i=0; i<levels; i++) {
		memcpy(ext_map[i], p, sizeof(map_gnode)*maxgroupnode);
		p+=maxgroupnode*sizeof(map_gnode);
	}

	/*Let's store in it the lost rnodes.*/
	if(emap_hdr->total_rblock_sz) {
		rblock=(map_rnode *)p;
		err=extmap_store_rblock(ext_map, levels, maxgroupnode, rblock,
				emap_hdr->rblock_sz);
		if(err!=levels) {
			error("unpack_extmap(): It was not possible to restore"
					" all the rnodes in the ext_map");
			free_extmap(ext_map, emap_hdr->quadg.levels, maxgroupnode);
			return 0;
		}
	}
	
	/* We restore the quadro_group struct */
	memcpy(quadg, &emap_hdr->quadg, sizeof(quadro_group));
	for(i=0; i<levels; i++)
		quadg->gnode[i]=gnode_from_pos(quadg->gid[i+1], ext_map[i]);

	/* Let's mark our gnodes ;) */
	for(i=1; i<emap_hdr->quadg.levels; i++) {
		ext_map[_EL(i)][quadg->gid[i]].flags&=~GMAP_VOID;
		ext_map[_EL(i)][quadg->gid[i]].g.flags&=~MAP_VOID;
		ext_map[_EL(i)][quadg->gid[i]].flags|=GMAP_ME;
		ext_map[_EL(i)][quadg->gid[i]].g.flags|=MAP_ME;
	}
	
	return ext_map;
}

/* * * save/load ext_map * * */
int save_extmap(map_gnode **ext_map, int maxgroupnode, quadro_group *quadg, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;
	
	/*Pack!*/
	pack=pack_extmap(ext_map, maxgroupnode, quadg, &pack_sz);
	if(!pack || !pack_sz)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the map in %s: %s", file, strerror(errno));
		return -1;
	}
	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);

	xfree(pack);
	fclose(fd);
	return 0;
}

map_gnode **load_extmap(char *file, quadro_group *quadg)
{
	map_gnode **ext_map;
	FILE *fd;
	struct ext_map_hdr emap_hdr;
	size_t pack_sz;
	char *pack;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the map from %s: %s", file, strerror(errno));
		return 0;
	}

	if(!fread(&emap_hdr, sizeof(struct ext_map_hdr), 1, fd))
		goto error;
	if(verify_ext_map_hdr(&emap_hdr))
		goto error;

	rewind(fd);
	pack_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.total_rblock_sz);
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto error;

	ext_map=unpack_extmap(pack, pack_sz, quadg);
	if(!ext_map)
		error("Cannot unpack the ext_map!");

	xfree(pack);
	fclose(fd);
	return ext_map;
error:
	error("Malformed ext_map file. Aborting load_extmap().");
	return 0;
}
