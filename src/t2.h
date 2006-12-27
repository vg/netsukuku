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

/*
 * Note: this file will substitute tracer.h
 */

#define MAX_TP_HOPS			MAXGROUPNODE

/*
 * TODO: wipe brdcast_hdr from the Netsukuku sources
 */

typedef struct
{
	u_char		link_id;

	rem_t		rem;		/* Route Efficiency Measure */
} tracer_hop;

/*
 * Usare la stessa tecnica di tpmask per memorizzare gli hop in un TP ?!? 
 */
