/* This file is part of Netsukuku
 * (c) Copyright 2007 Andrea Lo Pumo aka AlpT <alpt@freaknet.org>
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

#ifndef TRACER_H
#define TRACER_H

/*
 * Note: this file will substitute tracer.h
 */

#define MAX_TP_HOPS			MAXGROUPNODE

typedef struct
{
	u_char		link_id;

	rem_t		rem;		/* Route Efficiency Measure */
} tracer_hop;

/*
 * tpmask_t
 * ========
 *
 * Tracer Packet bitmask. The mask is formed by MAX_TP_HOPS bits and is
 * extracted from a tracer packet T.
 * If the i-th bit is set, then the node with id `i', was a hop of the 
 * T.
 *
 * This structure is used in {-map_node_t-}.
 *
 * Definitions
 * -----------
 *
 * The "tpmask of the route R" is the mask of the tracer packet which
 * has carried the route R.
 *
 * The "tpmask of the node N" is the mask of the tracer packet which
 * has carried, as hop, the node N.
 *
 * The "tpmask of the gateway G" is the mask of the tracer packet which
 * has carried, as last hop, the node G.
 *
 * Similarity						|{tp_similarity}|
 * ----------
 *
 * The similarity between two tpmasks is calculated as their hamming 
 * distance. The hamming distance between two binary string a and b, is
 * the number of ones in  a XOR b.
 *
 * distance = 0   	       -->  the two masks are identical, 
 *          = MAX_TP_HOPS/8-1  -->  the two masks are completely the opposite
 * 
 * See {-tp_mask_cmp-} and {-HAMD_MIN_EQ-}
 */
typedef struct
{
	u_char		mask[MAX_TP_HOPS/8];
} tpmask_t;

/*
 * Given two tpmask X and Y, they are considered equal if the following
 * condition is true:
 *	
 *	tp_mask_cmp(X, Y) <= HAMD_MIN_EQ(n)
 * 
 * where 
 *
 *	bX = count_bits(X)
 *	bY = count_bits(Y)
 *	n  = min(bX, bY)
 * 
 * See {-tp_is_similar-}
 */
#define HAMD_MIN_EQ(n)		((n)>>2)


/*\
 *
 *      * * *  Exported functions  * * *
 *
\*/

int tp_mask_cmp(tpmask_t a, tpmask_t b);
int tp_is_similar(tpmask_t X, tpmask_t Y);

#endif /*TRACER_H*/
