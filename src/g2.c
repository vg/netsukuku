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
 * gmap.c
 *
 * Code regarding the gnodes and the external map.
 *
 * For more information on the maps and the topology of Netsukuku see
 * {-CITE_TOPOLOGY_DOC-}
 */

#include "includes.h"

#include "inet.h"
#include "endianness.h"
#include "map.h"
#include "gmap.h"
#include "bmap.h"
#include "common.h"


/* 
 * get_groups
 * 
 * It returns how many groups there are in the given level.
 */
inline int get_groups(int max_levels, int lvl)
{ 				
	return lvl == max_levels ? 1 : MAXGROUPNODE;
}

/* 
 * gmap_is_gid_invalid
 * -------------------
 *
 * `gids' is a {-gidarray-}, describing a network. 
 * `gid' is a gnode ID of level `lvl'.
 *
 * This subroutine returns 1 if `gid' is considered invalid or cannot be used
 * to form a regular IP.
 */
int gmap_is_gid_invalid(gid_t *gids, gid_t gid, int lvl, int family)
{
	if(family == AF_INET) {
		if(lvl == GET_LEVELS(family)-1) {
			if(!gid) /* ZERONET */
				return 1; 
			
			if(gid >= 224 && gid <= 255)
				/* MULTICAST and BADCLASS */
				return 1;
			
			if(gid == 127) /* LOOPBACK */
				return 1;
			
			if(gid == 192 && gids[lvl-1] == 168)
				/* 192.168.x.x is private, we cannot use IP of
				 * that range */
				return 1;

			if((gid == 172 && (gids[lvl-1] >= 16 || gids[lvl-1] <= 31)) &&
				!restricted_mode)
				/* We aren't in restricted mode, so
				 * 172.16.0.0-172.31.255.255 is a private
				 * class */
				return 1;

			if(restricted_mode && 
			      (
				(restricted_class == RESTRICTED_10 && gid != 10) ||
				(
				 restricted_class == RESTRICTED_172 && 
				 	!(gid == 172 && (gids[lvl-1] >= 16 ||
							gids[lvl-1] <= 31))
				)
			      )
			  )
				/* We are in restricted mode, thus this IP is
				 * invalid because it isn't in the 10.x.x.x or
				 * 172.(16-31).x.x format */
				return 1;

		} else if(lvl == GET_LEVELS(family)-2) {
			if(gid == 168 && gids[lvl+1] == 192)
				/* 192.168.x.x */
				return 1;
			if(((gid >= 16 || gid <= 31) && gids[lvl+1] == 172) &&
				!restricted_mode)
				/* 172.16.0.0-172.31.255.255 */
				return 1;
		}
	} else if(family == AF_INET6) {
		/* TODO: nothing ? */
		return 0;
	}
	
	return 0;
}

/*\
 *
 *     * * *  Conversion functions  * * *
 *
\*/


/*
 * gmap_gnode2pos
 * --------------
 *
 * `map' is a pointer to the start of a {-single extmap-}
 * `gnode' is a pointer to a gnode of `map'.
 *
 * This subroutine converts `gmap' to its array position in `map', and returns
 * the result.
 * The array position corresponds to the gnode id of `gnode'.
 *
 * See also {-extmap-}.
 */
gid_t gmap_gnode2pos(map_gnode *gnode, map_gnode *map)
{
	return (gid_t)(((char *)gnode-(char *)map)/sizeof(map_gnode));
}

/*
 * gmap_pos2gnode
 * --------------
 *
 * The inverse of {-gmap_gnode2pos-}
 */
map_gnode *gmap_pos2gnode(gid_t pos, map_gnode *map)
{
	return (map_gnode *)((pos*sizeof(map_gnode))+(char *)map);
}

/* 
 * gmap_ip2gid
 * -----------
 * 
 * Converts `ip' to the gnode id of the specified `level'.
 * The gid is then returned.
 *
 * Note: this function cannot fail! So be sure to pass a valid `level'.
 */
gid_t gmap_ip2gid(inet_prefix *ip, int level)
{
	u_char *h_ip=(u_char *)ip->data;
	gid_t gid;

	/* 
	 * The formula is:
	 * gid=(ip/MAXGROUPNODE^level) - (ip/MAXGROUPNODE^(level+1) * MAXGROUPNODE);
	 * but since we have a MAXGROUPNODE equal to 2^8 we can just return
	 * the `level'-th byte of ip.data.
	 */
#if BYTE_ORDER == LITTLE_ENDIAN
	gid=(gid_t)h_ip[level];
#else
	gid=(gid_t)h_ip[GET_LEVELS(ip->family)-level-1];
#endif

	return gid;	
}

/* 
 * gmap_ip2gids
 * ------------
 *
 * it fills the `gid' array which has `levels'# members with
 * 	gid[x] = gmap_ip2gid(`ip', x);
 */
void gmap_ip2gids(inet_prefix *ip, gid_t *gid, int levels)
{
	int i;
	for(i=0; i<levels; i++)
		gid[i]=gmap_ip2gid(ip, i);
}

/* 
 * gmap_gids2ip
 * ------------
 * 
 * It sets in `*ip' the {-ipstart_t-} of the network described by the `gid'
 * array.
 * The `gid' array has `total_levels'# members. The member gid[x] is the gid
 * of the gnode of level x.
 *
 * `levels' is the number of array elements considered.
 * Only the elements going from gid[total_levels-levels] to gid[total_levels-1],
 * will be considered, the other will be ignored.
 *
 * `family' is used to fill the inet_prefix of ipstart.
 *
 * `ip'->bits is set to `levels'*8, thus respecting the CIDR format (see
 * {-ipstart_t-}).
 */
void gmap_gids2ip(gid_t *gid, u_char total_levels, uint8_t levels, int family,
			inet_prefix *ip)
{
	int i, h_ip[MAX_IP_INT];
	u_char *ipstart;

	setzero(h_ip, MAX_IP_SZ);
	ipstart=(u_char *)h_ip;

	for(i=total_levels-ZERO_LEVEL; i >= total_levels-levels; i--) {
		/* The formula is:
		 * ipstart += MAXGROUPNODE^i * gid[i]; 
		 * but since MAXGROUPNODE is equal to 2^8 we just set each
		 * single byte of ipstart. */

#if BYTE_ORDER == LITTLE_ENDIAN
		ipstart[i]=(u_char)gid[i];
#else
		ipstart[GET_LEVELS(family)-i-1]=(u_char)gid[i];
#endif
		
	}
	
	memcpy(ip->data, h_ip, MAX_IP_SZ);
	ip->family=family;
	ip->len = (family == AF_INET) ? 4 : 16;
	ip->bits = levels*8;
}

/* 
 * gmap_ip2nnet
 * ------------
 * 
 * Using the given `ip' it fills the `nn' nodenet_t struct. The given `flags'
 * specify what elements will be set in the struct, see {-ip2nnet-flags-}.
 */
void gmap_ip2nnet(inet_prefix ip, map_gnode **ext_map, nodenet_t *nn, char flags)
{
	int i;
	u_char levels;
	git_t gid[MAX_LEVELS];

	setzero(nn, sizeof(nodenet_t));
	
	levels=GET_LEVELS(ip.family);
	nn->levels=levels;

	if(flags & NNET_GID) {
		gmap_ip2gids(&ip, nn->gid, levels);
		memcpy(gid, nn->gid, sizeof(gid));
		
		if(flags & NNET_IPSTART)
			for(i=0; i<levels; i++)
				gmap_gids2ip(gid, levels, levels-i, ip.family,
						&nn->ipstart[i]);
	}
	if(flags & NNET_GNODE) {
		for(i=0; i<levels-ZERO_LEVEL; i++)
			nn->gnode[i]=gmap_pos2gnode(nn->gid[i+1], ext_map[i]);
		nn->gnode[levels-ZERO_LEVEL]=&ext_map[i][0];
	}
}


/*\
 *
 *     * * *  Nodenet functions  * * *
 *
\*/


/*
 * gnode_seeds_inc
 * ---------------
 * 
 * it increments the seeds counter in the `gnode' gnode,
 * setting the appropriate flag if it is full.
 */
void gnode_seeds_inc(map_gnode *gnode)
{
	if(gnode->seeds < MAXGROUPNODE-1)
		gnode->seeds++;
	if(gnode->seeds == MAXGROUPNODE-1)
		gnode->flags|=GMAP_FULL;
}

/*
 * gnode_seeds_dec
 * ---------------
 * 
 * the same of gnode_seeds_inc, but it decrements instead.
 */
void gnode_seeds_dec(map_gnode *gnode)
{
	if(gnode->seeds-1 >= 0) 
		gnode->seeds--;
	gnode->flags&=~GMAP_FULL;
}
	

/*
 * nnet_setflags
 * -------------
 *
 * Set the given `flags' to nn->gnode[*].flags.
 */
void nnet_setflags(nodenet_t *nn, char flags)
{
	map_gnode *gnode;
	int i;

	for(i=1; i < nn->levels; i++)
		if((gnode=nn->gnode[_EL(i)]))
			gnode->g.flags|=flags;
}

void nnet_reset(nodenet_t *nn)
{
	setzero(nn, sizeof(nodenet_t));
}

void nnet_free(nodenet_t *nn)
{
	nnet_reset(nn);
	xfree(nn);
}

/*
 * nnet_seeds_inc
 * ---------------
 * 
 * It assumes that a new (g)node has been added in level `level' and
 * increments by one the seeds counter of the gnode of level `level'+1.
 * If the gnode becomes full, the appropriate flag will be set.
 * 
 * `nn' is the {-nodenet_t-} struct describing the network where the addition
 * happened.
 */
void nnet_seeds_inc(nodenet_t *nn, int level)
{
	if(level >= nn->levels-1)
		return;
	
	gnode_seeds_inc(nn->gnode[_EL(level+1)]);
}

/*
 * nnet_seeds_dec
 * ---------------
 * 
 * the same of nnet_seeds_inc, but it decrements instead.
 */
void nnet_seeds_dec(nodenet_t *nn, int level)
{
	if(level >= nn->levels-1)
		return;
	
	gnode_seeds_dec(nn->gnode[_EL(level+1)]);
}


/*
 * nnet_pack
 * ---------
 * 
 * packs the `nn' nodenet_t struct and stores it in `pack', which must be 
 * NODENET_PACK_SZ bytes big. `pack' will be in network order.
 */
void nnet_pack(nodenet_t *nn, char *pack)
{
	struct nodenet_pack *npack;
	char *buf;
	int i;

	buf=pack;
	npack=(struct nodenet_pack *)pack;

	npack->levels = nn->levels;
	memcpy(npack->gid, nn->gid, sizeof(nn->gid));

	for(i=0; i<MAX_LEVELS; i++)
		inetp_pack(&nn->ipstart[i], (char *)&npack->ipstart_pack[i]);
}

/*
 * nnet_unpack
 * -----------
 * 
 * Restores in `nn' the nodenet_t struct contained in `pack'.
 * Note that `pack' will be modified during the restoration.
 */
void nnet_unpack(nodenet_t *nn, char *pack)
{
	struct nodenet_pack *npack;
	char *buf;
	int i;

	buf=pack;
	npack=(struct nodenet_pack *)pack;

	nn->levels = npack->levels;
	memcpy(nn->gid, npack->gid, sizeof(npack->gid));

	for(i=0; i<MAX_LEVELS; i++)
		inetp_unpack(&nn->ipstart[i], (char *)&npack->ipstart_pack[i]);
}

/*
 * Functions used by nnet_gids_inc(), see below
 */

int is_map_void_flag_set(map_node *node)
{
	return node->flags & MAP_VOID;
}

int is_gmap_full_flag_set(map_gnode *gnode)
{
	return gnode->flags & GMAP_FULL;
}

int is_gmap_void_flag_set(map_gnode *gnode)
{
	return gnode->flags & GMAP_VOID;
}

int isnot_gmap_void_flag_set(map_gnode *gnode)
{
	return !is_gmap_void_flag_set(gnode);
}

/*
 * nnet_gids_inc
 * -------------
 * 
 * It randomly increments the members of the `nn'->gid array 
 * 	until 
 * 		all its gids point to gnodes which don't have a 
 * 		particular gnode->flag set
 * 	    and 
 * 	        the gid[0] point to a node which does have a particular 
 * 	        node->flag set.
 * 
 * In order to verify that a gnode doesn't have the flag set the
 * `is_gnode_flag_set' function is called, the same is done for the nodes with
 * the `is_node_flag_set' function. 
 * These functions must return 1 if the flag is set.
 * 
 * nnet_gids_inc() starts from the nn->gid[`level'] member and finishes to 
 * nn->gid[nn->levels-1].
 *
 * If no suitable configuration is found -1 is returned.
 *
 * It's assumed that `ext_map' and `int_map' are the maps relative to the
 * `nn' nodenet_t.
 *
 * For examples, see {-gids_find_free-} and {-gids_find_void-}.
 */
int nnet_gids_inc(nodenet_t *nn, int level,
			map_gnode **ext_map, 
			map_node *int_map, 
			int(*is_gnode_flag_set)(map_gnode *gnode), 
			int(*is_node_flag_set)(map_node *node))
{
	int groups, i, e=0, o, family;
	gid_t g, gid;

	if(level >= nn->levels)
		return -1;

	family = nn->ipstart[level].family;
	
	g  = (level == nn->levels ? 0 : nn->gid[level]);
	gid= nn->gid[level];
	groups=get_groups(nn->levels, level);
	
	if((!level && !is_node_flag_set(&int_map[gid])) ||
			(level && is_gnode_flag_set(&ext_map[_EL(level)][g]))) {	

		int _check_gid()
		{
			map_node *node;
			map_gnode *gnode;

			nn->gid[level]=(gid + i) % groups;

			if(gmap_is_gid_invalid(nn->gid, nn->gid[level], level, family))
				return 0;

			node  = &int_map[nn->gid[level]];
			gnode = &ext_map[_EL(level)][nn->gid[level]];

			if((!level && is_node_flag_set(node)) ||
				(level && !is_gnode_flag_set(gnode))) {
				return 1;
			}
		}

		/*
		 * find a gid in this `level' which has the node flag set or
		 * the gnode flag not set
		 */
		e=0;
		for(o=i=rand_range_fast(0, MAXGROUPNODE-1); i < groups; i++)
			if(_check_gid()) {
				e=1;
				break;
			}
		if(!e)
		  for(i=0; i < o; i++)
			if(_check_gid()) {
				e=1;
				break;
			}


		/*
		 * not a single gid was found
		 */
		if(!e) {
			g = (level+1 == nn->levels ? 0 : nn->gid[level+1]);

			if((is_gmap_full_flag_set == is_gnode_flag_set) && 
					!(ext_map[_EL(level+1)][g].flags & GMAP_FULL)) {
				/* 
				 * There is a logical contradiction here:
				 * we didn't find any free (g)nodes in this
				 * level, but the upper gnode at level+1,
				 * which is the parent of this level, isn't
				 * marked as full! So what's happening here?
				 * Ignore this absurd and mark it as full.
				 */
				ext_map[_EL(level+1)][g].flags|=GMAP_FULL;
				debug(DBG_NORMAL, ERROR_MSG "logical "
						"contradiction detected",
						ERROR_POS);
			}

			/*
			 * Recurse by leveling up
			 */
			if(!nnet_gids_inc(nn, level+1, ext_map, int_map, 
					is_gnode_flag_set, is_node_flag_set))
				/* 
				 * We changed one of our upper gid, we can
				 * retake the old gid we had at this `level'
				 */
				nn->gid[level]=gid;
			else
				/*
				 * It's all full!
				 */
				return -1;
		}
	}

	return 0;
}

/*
 * gids_find_free
 * --------------
 * 
 * it uses nnet_gids_inc() to choose gids which don't point to FULL gnodes.
 */
int gids_find_free(nodenet_t *nn, int level, map_gnode **ext_map,
		map_node *int_map)
{
	return nnet_gids_inc(nn, level, ext_map, int_map, 
			is_gmap_full_flag_set, is_map_void_flag_set);
}

/*
 * gids_find_void
 * --------------
 * 
 * it uses nnet_gids_inc() to choose gids which point to VOID gnodes.
 */
int gids_find_void(nodenet_t *nn, int level, map_gnode **ext_map,
		map_node *int_map)
{
	return nnet_gids_inc(nn, level, ext_map, int_map, 
			isnot_gmap_void_flag_set, is_map_void_flag_set);
}

/*
 * gmap_random_ip
 * --------------
 * 
 * It generates a new random ip. 
 *
 * If `ipstart' is not NULL the new ip is restricted in the `final_gid' of
 * `final_level'.
 * For example, with ipstart = 11.22.33.44, final_level = 1, final_gid = 55,
 * the new ip X will be inside the range:
 * 		11.22.55.0/24 <= X < 11.22.56.0/24
 * instead, with final_level=2:
 * 		11.55.0.0/16  <= X < 11.56.0.0/16
 * 		
 * If `ipstart' is NULL a completely random ip is generated.
 *
 * `total_levels' is the maximum number of levels.
 * `ext_map' is an external map, used to recognize empty gnodes.
 *
 * If `only_free_gnode' is not 0, only the available and empty gnode are chosen.
 * In this case -1 may be returned if there aren't any free gnode to choose.
 *
 * The new ip is stored in `new_ip' and on success 0 is returned.
 */
int gmap_random_ip(inet_prefix *ipstart, int final_level, gid_t final_gid, 
			int total_levels, map_gnode **ext_map, int only_free_gnode, 
			inet_prefix *new_ip, int my_family)
{
	int i, e, x, level;
	nodenet_t nn;

	setzero(new_ip, sizeof(inet_prefix));
	
	if(!ipstart || final_level==total_levels) {
		/*
		 * Let's choose a completely random ip.
                 */
		new_ip->family = my_family;
		inet_random_ip(new_ip);

		if(!only_free_gnode)
			return 0;
		else
			gmap_ip2nnet(*new_ip, ext_map, &nn, NNET_GID);
	} else {
		/*
		 * We can choose only a random ip which is inside the final_gid.
		 * `final_gid' is a gnode of the `final_level' level.
		 * `final_level' has its `ipstart'; with it we determine
		 * its higher levels.
		 * The work is done in this way:
		 *   - ipstart is splitted in gids and they are placed in nn.gid.
		 *   - The gids of levels higher than `final_level' are left untouched.
		 *   - The final_level gid is set to `final_gid'.
		 *   - The gids of levels lower than `final_level' are chosen
		 *     randomly.
		 *   - The ipstart is recomposed from the gids.
		 */
		gmap_ip2nnet(*ipstart, ext_map, &nn, NNET_GID);
		nn.gid[final_level]=final_gid;

		/* Choose random gids for each level lower than `final_level'. */
		for(level=final_level-1; level >=0; level--)
			nn.gid[final_level]=rand_range(0, MAXGROUPNODE-1);
	}

	if(only_free_gnode)
		/* Change the gids if some of them point to full gnodes */
		gids_find_free(&nn, 0, ext_map, 0);

	/* 
	 * Ok, we've set the gids of each level so we recompose them in the
	 * new_ip.
	 */
	gmap_gids2ip(nn.gid, total_levels, total_levels, my_family, new_ip);

	return 0;
}

/*
 * gids_cmp
 * --------
 * 
 * compares the two `gids_a' and `gids_b' arrays starting from the
 * `lvl'th member to the `max_lvl-1'th
 * If the gids compared are the same, zero is returned.
 */
int gids_cmp(gid_t *gids_a, gid_t *gids_b, int lvl, int max_lvl)
{
	int i;

	for(i=lvl; i < max_lvl; i++)
		if(gids_a[i] != gids_b[i])
			return 1;
	return 0;

}

/* 
 * nnet_gids_cmp
 * -------------
 * 
 * Compares the gids of `a' and `b' starting from the `lvl'th
 * level. If the gids compared are the same, zero is returned.
 */
int nnet_gids_cmp(nodenet_t a, nodenet_t b, int lvl)
{
	if(a.levels != b.levels)
		return 1;

	return gids_cmp(a.gid, b.gid, lvl, a.levels);
}

/*
 * inetp_gids_cmp
 * --------------
 * 
 * a wrapper to nnet_gids_cmp() that takes inet_prefixes as argvs.
 */
int inetp_gids_cmp(inet_prefix a, inet_prefix b, int lvl)
{
	nodenet_t qa, qb;

	gmap_ip2nnet(a, 0, &qa, NNET_GID);
	gmap_ip2nnet(b, 0, &qb, NNET_GID);

	return nnet_gids_cmp(qa, qb, lvl);
}


/*\
 *
 *     * * *  External map functions  * * *
 *
\*/

/*
 * gmap_gnode_alloc
 * ----------------
 *
 * Allocates the components of the `gnode' map_gnode struct.
 */
void gmap_gnode_alloc(map_gnode *gnode)
{
	gnode->flags|=GMAP_VOID;
	map_node_alloc(&gnode->g);
}

/*


 * gmap_gnode_del
 * --------------
 *
 * Frees what has been allocated with {-gmap_gnode_alloc-}
 */
void gmap_gnode_del(map_gnode *gnode)
{
	map_node_del(&gnode.g);
	setzero(gnode, sizeof(*gnode));
}


/*
 * gmap_alloc
 * ----------
 *
 * Allocates a {-single extmap-}, formed by `groups' map_gnode structs,
 * and returns the pointer to its start.
 *
 * If `groups' is zero, then MAXGROUPNODE structs are allocated.
 */
map_gnode *gmap_alloc(int groups)
{
	map_gnode *gmap;
	size_t len;
	int i;
	
	if(!groups)
		groups=MAXGROUPNODE;

	len=sizeof(map_gnode) * groups;
	gmap=(map_gnode *)xzalloc(len);

	for(i=0; i<groups; i++)
		gmap_gnode_alloc(&gmap[i]);

	return gmap;
}

/*
 * gmap_free
 * ---------
 *
 * Frees what has been allocated with {-gmap_alloc-}.
 */
void gmap_free(map_gnode *gmap, int groups)
{
	int i;

	if(!groups)
		groups=MAXGROUPNODE;

	for(i=0; i<groups; i++)
		gmap_gnode_del(&gmap[i]);

	xfree(gmap);
}

/*
 * gmap_gnode_reset
 * ----------------
 *
 * Resets the map_gnode struct pointed by `gnode'.
 * See also {-map_node_reset-} for more details.
 */
void gmap_gnode_reset(map_gnode *gnode)
{
	map_node_reset(&gnode.g);
	gnode->flags=GMAP_VOID;
	gnode->seeds=gnode->gcount=0;
}

/*
 * gmap_reset
 * ----------
 *
 * Resets the entire `gmap', which is a{-single extmap-}, formed by
 * `groups'# structs.
 *
 * If `groups' is zero, then the first MAXGROUPNODE structs are reset.
 */
void gmap_reset(map_gnode *gmap, int groups)
{
	int i;
	size_t len;

	if(!groups)
		groups=MAXGROUPNODE;
	len=sizeof(map_gnode)*groups;
	
	for(i=0; i<groups; i++)
		gmap_gnode_reset(&gmap[i]);
}

/* extmap_alloc
 * ------------
 *
 * Initialize an {-extmap-} by allocating `levels'# gmaps. Each gmap
 * has `groups'# elements.
 * If `groups' is zero, then it is considered to be equal to MAXGROUPNODE.
 */
map_gnode **extmap_alloc(u_char levels, int groups)
{
	map_gnode **ext_map;
	int i;

	if(!levels)
		levels=MAX_LEVELS;
	if(!groups)
		groups=MAXGROUPNODE;

	ext_map=(map_gnode **)xmalloc(sizeof(map_gnode *) * (levels));
	levels-=ZERO_LEVEL; /*We strip off the Zero level*/
	for(i=0; i<levels; i++)
		ext_map[i]=gmap_alloc(groups);
	
	/*Ok, now we stealthy add the unity_level which has only one gmap.*/
	ext_map[levels]=gmap_alloc(1);
	return ext_map;
}

/* 
 * extmap_free
 * -----------
 *
 * Destroy the {-extmap-} allocated with {-extmap_alloc-}
 */
void extmap_free(map_gnode **ext_map, u_char levels, int groups)
{
	int e;

	if(!levels)
		levels=MAX_LEVELS;
	if(!groups)
		groups=MAXGROUPNODE;

	levels--;
	for(e=0; e<levels; e++) {
		gmap_free(ext_map[e], 0);
		ext_map[e]=0;
	}
	/*Free the unity_level map*/
	gmap_free(ext_map[levels], 1);
	ext_map[levels]=0;

	xfree(ext_map);
}

/*
 * extmap_reset
 * ------------
 *
 * Resets the entire `ext_map'.
 */
void extmap_reset(map_gnode **ext_map, u_char levels, int groups)
{
	int i;
	
	for(i=1; i<levels; i++) 
		gmap_reset(ext_map[_EL(i)], groups);
	gmap_reset(ext_map[_EL(i)], 1);
}

/*
 * gmap_gw_find
 * ------------
 *
 * A wrapper to {-map_gw_find-}
 */
map_gw *gmap_gw_find(map_gnode *gnode, map_gnode *n)
{
	return map_gw_find(&gnode.g, (map_node *)n);
}


/* 
 * extmap_find_level
 * -----------------
 *
 * It returns the position of the gnode map which contains `gnode'. 
 * This position corresponds to the level of that gmap.
 * The ext_map is given in `ext_map`. 
 * `max_level' is the maximum number of levela present in the `ext_map'.
 * 
 * Example: if `gnode' is in ext_map[i] it will return i; 
 *
 * On failure -1 is returned.
 */
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

/*\
 *
 *     * * *  Packing functions  * * *
 *
\*/


/*
 * merge_lvl_ext_maps
 * 
 * merges two ext_maps of a specific `level'. It is used by merge_ext_maps(),
 * see below.
 * This function is the exact replica of merge_maps() in map.c, that's why it
 * isn't commented.
 */
int merge_lvl_ext_maps(map_gnode *base, map_gnode *new, nodenet_t base_root,
		nodenet_t new_root, int level)
{
	map_gnode *gnode_gw, *new_root_in_base;
	int base_root_pos, ngpos;
	u_int base_trtt, new_trtt;
	int i, e, x;

	new_root_in_base=&base[new_root.gid[level]];
	base_root_pos=base_root.gid[level];

	for(i=0; i<MAXGROUPNODE; i++) {
		if(base[i].g.flags & MAP_ME 	  || new[i].g.flags & MAP_ME ||
			new[i].g.flags & MAP_VOID || base[i].flags & GMAP_ME || 
			new[i].flags & GMAP_ME    || new[i].flags & GMAP_VOID)
			continue;

		for(e=0; e<new[i].g.links; e++) {
			gnode_gw=(map_gnode *)new[i].g.r_node[e].r_node; 
			
			ngpos=gmap_gnode2pos(gnode_gw, new);
			if(ngpos == base_root_pos)
				continue;

			if(new[i].g.flags & MAP_RNODE)
				new[i].g.r_node[e].r_node=(int *)new_root_in_base;
			else if(base[ngpos].g.flags & MAP_VOID || 
				base[ngpos].flags & GMAP_VOID || !base[ngpos].g.links)
				new[i].g.r_node[e].r_node=(int *)new_root_in_base;
			else
				new[i].g.r_node[e].r_node=base[ngpos].g.r_node[0].r_node;
						
			if(e >= base[i].g.links) {
				rnode_add(&base[i].g, &new[i].g.r_node[e]);
				rnode_trtt_order(&base[i].g);
				base[i].g.flags|=MAP_UPDATE;
				continue;
			}
		
			base_trtt = get_route_trtt(&base[i].g, base[i].g.links-1);
			new_trtt  = get_route_trtt(&new[i].g, e);
			if(base_trtt < new_trtt)
				continue;
		
			for(x=0; x<base[i].g.links; x++) {
				base_trtt = get_route_trtt(&base[i].g, x);
				new_trtt  = get_route_trtt(&new[i].g, e);
				if(base_trtt > new_trtt) {
					map_rnode_insert(&base[i].g, x, &new[i].g.r_node[e]);
					base[i].g.flags|=MAP_UPDATE;
					break;
				}
			}
		}
		
		if(base[i].g.links) {
			base[i].g.flags&=~MAP_VOID;
			base[i].flags&=~GMAP_VOID;
		} else
			gmap_node_del(&base[i]);
	}

	return 0;
}

/*
 * merge_ext_maps
 * 
 * it fuses the `base' and `new' ext_maps generating a single
 * ext_map which has the best routes. The generated map is stored in `base'
 * `base_root' is the nodenet_t related to `base'.
 * `new_root' is the nodenet_t of the `new' ext_map.
 * On error -1 is returned.
 */
int merge_ext_maps(map_gnode **base, map_gnode **new, nodenet_t base_root,
		nodenet_t new_root) 
{
	int level, i;

	for(level=base_root.levels-1; level >= 0; level--) {
		
		if(base_root.gid[level] != base_root.gid[level])
			break;

		if(level == 1)
			/* The two maps are of the same nodenet_t */
			return -1;
	}
	
	for(i=level; i < base_root.levels; i++)
		merge_lvl_ext_maps(base[_EL(i)], new[_EL(i)], base_root, 
				new_root, i);

	return 0;
}

/* 
 * gmap_get_rblock
 * 
 * It uses get_rnode_block to pack all the ext_map's rnodes
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
 * extmap_get_rblock
 * 
 * It packs the rnode_block for each map present in the `ext_map'.
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

/* 
 * verify_ext_map_hdr
 * 
 * It verifies the validity of an ext_map_hdr.
 * `nnet' is the unpacked emap_hdr->nnet nodenet_t.
 */
int verify_ext_map_hdr(struct ext_map_hdr *emap_hdr, nodenet_t *nnet)
{
	u_char levels;
	int maxgroupnode, i;
	
	levels=nnet->levels-UNITY_LEVEL;
	maxgroupnode=emap_hdr->ext_map_sz/(MAP_GNODE_PACK_SZ*levels);
	if(levels > MAX_LEVELS || maxgroupnode > MAXGROUPNODE || 
			emap_hdr->total_rblock_sz > MAXRNODEBLOCK_PACK_SZ*levels ||
			emap_hdr->ext_map_sz > maxgroupnode*MAP_GNODE_PACK_SZ*levels)
		return 1;

	for(i=0; i<levels; i++)
		if(nnet->gid[i] > maxgroupnode)
			return 1;

	return 0;
}

void extmap_free_rblock(map_rnode **rblock, u_char levels)
{
	int i;
	for(i=0; i<levels; i++)
		if(rblock[i])
			xfree(rblock[i]);
	xfree(rblock);
}

/*
 * pack_map_gnode
 * 
 * packs the `nn' map_gnode struct and stores it in
 * `pack', which must be MAP_GNODE_PACK_SZ bytes big. `pack' will be in
 * network order.
 */
void pack_map_gnode(map_gnode *gnode, char *pack)
{
	char *buf;

	buf=pack;

	pack_map_node(&gnode->g, buf);
	buf+=MAP_NODE_PACK_SZ;

	memcpy(buf, &gnode->flags, sizeof(char));
	buf+=sizeof(char);

	memcpy(buf, &gnode->seeds, sizeof(char));
	buf+=sizeof(char);

	memcpy(buf, &gnode->gcount, sizeof(u_int));
	buf+=sizeof(u_int);

	ints_host_to_network(pack, map_gnode_iinfo);
}

/*
 * unpack_map_gnode
 * 
 * restores in `nn' the map_gnode struct contained in `pack'.
 * Note that `pack' will be modified during the restoration.
 */
void unpack_map_gnode(map_gnode *gnode, char *pack)
{
	char *buf;

	buf=pack;

	ints_nodenet_to_host(pack, map_gnode_iinfo);
	
	unpack_map_node(&gnode->g, buf);
	buf+=MAP_NODE_PACK_SZ;

	memcpy(&gnode->flags, buf, sizeof(char));
	buf+=sizeof(char);

	memcpy(&gnode->seeds, buf, sizeof(char));
	buf+=sizeof(char);

	memcpy(&gnode->gcount, buf, sizeof(u_int));
	buf+=sizeof(u_int);
}

/* 
 * extmap_pack
 * 
 * It returns the packed `ext_map', ready to be saved or sent. It stores 
 * in `pack_sz' the size of the package. Each gmaps, present in the `ext_map', has 
 * `maxgroupnode' nodes. `nnet' must be a valid nodenet_t struct filled with 
 * valid values. 
 */
char *extmap_pack(map_gnode **ext_map, int maxgroupnode, nodenet_t *nnet, size_t *pack_sz)
{
	struct ext_map_hdr emap_hdr;
	map_rnode **rblock;
	int *count, i, e;
	char *package, *p=0;
	u_char levels=nnet->levels-UNITY_LEVEL;

	/*Packing the rblocks*/
	rblock=extmap_get_rblock(ext_map, levels, maxgroupnode, &count);

	/*Building the hdr...*/
	setzero(&emap_hdr, sizeof(struct ext_map_hdr));
	emap_hdr.ext_map_sz=maxgroupnode*MAP_GNODE_PACK_SZ*levels;
	for(i=0; i<levels; i++) {
		emap_hdr.rblock_sz[i]=count[i]*MAP_RNODE_PACK_SZ;
		emap_hdr.total_rblock_sz+=emap_hdr.rblock_sz[i];
	}
	
	nnet_pack(nnet, emap_hdr.nnet);

	/*Let's fuse all in one*/
	*pack_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.total_rblock_sz);
	package=xmalloc(*pack_sz);
	
	memcpy(package, &emap_hdr, sizeof(struct ext_map_hdr));
	ints_host_to_network(package, ext_map_hdr_iinfo);

	p=package+sizeof(struct ext_map_hdr);
	for(i=0; i<levels; i++) {
		for(e=0; e<maxgroupnode; e++) {
			pack_map_gnode(&ext_map[i][e], p);
			p+=MAP_GNODE_PACK_SZ;
		}
	}
	
	/* If the rblock is not null copy it in the `package' */
	if(rblock) {
		for(i=0; i<levels; i++) {
			if(!emap_hdr.rblock_sz[i])
				continue;
			memcpy(p, rblock[i], emap_hdr.rblock_sz[i]);
			p+=emap_hdr.rblock_sz[i];
		}
		extmap_free_rblock(rblock, levels);
	}

	xfree(count);
	return package;
}


/* 
 * extmap_unpack
 * 
 * Given a valid ext_map package (packed with extmap_pack), it 
 * allocates a brand new ext_map and restores in it the gmaps and the rnodes.
 * In `nnet' is stored the nodenet_t referring to this ext_map.
 * On success the a pointer to the new ext_map is retuned, otherwise 0 will be
 * the fatal value.
 * Note: `package' will be modified during the unpacking.
 */
map_gnode **extmap_unpack(char *package, nodenet_t *nnet)
{
	map_gnode **ext_map;
	struct ext_map_hdr *emap_hdr=(struct ext_map_hdr *)package;
	map_rnode *rblock;
	u_char levels;
	int err, i, e, maxgroupnode;
	char *p;

	ints_nodenet_to_host(emap_hdr, ext_map_hdr_iinfo);
	nnet_unpack(nnet, emap_hdr->nnet);
	
	levels=nnet->levels-UNITY_LEVEL;
	maxgroupnode=emap_hdr->ext_map_sz/(MAP_GNODE_PACK_SZ*levels);
	
	if(verify_ext_map_hdr(emap_hdr, nnet)) {
		error("Malformed ext_map_hdr. Aborting unpack_map().");
		return 0;
	}
	
	/*Unpacking the ext_map*/
	p=package+sizeof(struct ext_map_hdr);
	ext_map=extmap_alloc(nnet->levels, maxgroupnode);
	for(i=0; i<levels; i++) {
		for(e=0; e<maxgroupnode; e++) {
			unpack_map_gnode(&ext_map[i][e], p);
			p+=MAP_GNODE_PACK_SZ;
		}
	}

	/*Let's store in it the lost rnodes.*/
	if(emap_hdr->total_rblock_sz) {
		rblock=(map_rnode *)p;
		err=extmap_store_rblock(ext_map, levels, maxgroupnode, rblock,
				emap_hdr->rblock_sz);
		if(err!=levels) {
			error("extmap_unpack(): It was not possible to restore"
					" all the rnodes in the ext_map");
			extmap_free(ext_map, nnet->levels, maxgroupnode);
			return 0;
		}
	}
	
	/* We restore the nodenet_t struct */
	for(i=0; i<levels; i++)
		nnet->gnode[i]=gmap_pos2gnode(nnet->gid[i+1], ext_map[i]);

	return ext_map;
}

/* * * save/load ext_map * * */
int extmap_save(map_gnode **ext_map, int maxgroupnode, nodenet_t *nnet, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;
	
	/*Pack!*/
	pack=extmap_pack(ext_map, maxgroupnode, nnet, &pack_sz);
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

map_gnode **extmap_load(char *file, nodenet_t *nnet)
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

	ints_nodenet_to_host(&emap_hdr, ext_map_hdr_iinfo);
	nnet_unpack(nnet, emap_hdr.nnet);
	if(verify_ext_map_hdr(&emap_hdr, nnet))
		goto error;

	rewind(fd);
	pack_sz=EXT_MAP_BLOCK_SZ(emap_hdr.ext_map_sz, emap_hdr.total_rblock_sz);
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto error;

	ext_map=extmap_unpack(pack, nnet);
	if(!ext_map)
		error("Cannot unpack the ext_map!");

	xfree(pack);
	fclose(fd);
	return ext_map;
error:
	error("Malformed ext_map file. Aborting extmap_load().");
	return 0;
}
