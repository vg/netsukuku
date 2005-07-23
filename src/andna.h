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

#define MAX_ANDNA_BACKUP_GNODES		2
#define MAX_ANDNA_QUEUE			5

/* Returns the number of nodes to be used in a backup_gnode */
#define ANDNA_BACKUP_NODES(seeds)	({(seeds) > 8 ? 			\
					  ((seeds)*32)/MAXGROUPNODE : (seeds);})
				

/*** andna_cache flags ***/
#define ANDNA_BACKUP	1		/* We are a backup_node */
#define ANDNA_COUNTER	(1<<1)		/* We are a counter_node */
#define ANDNA_FULL	(1<<2)		/* Queue full */

char andna_flags;

struct andna_cache
{
	struct andna_cache *next;
	struct andna_cache *prev;

	char 		*hash;		/* hostname's hash */
	inet_prefix	rip;		/* register_node ip */
	time_t		timestamp;
	RSA		*pubkey;
	char 		flags;
};

struct andna_cache *andna_c;		/* The linked list head */
int andna_c_counter;
