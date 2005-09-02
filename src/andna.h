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

/*
 * * * ANDNA requests/replies pkt stuff * * * 
 */

/* * * andna pkt flags * * */
#define ANDNA_UPDATE		1		/* Update the hostname */
#define ANDNA_FORWARD		(1<<1)		/* Forward this pkt, plz */
#define ANDNA_REV_RESOLVE	(1<<2)		/* Give me your hostnames */

/*
 * Andna registration request pkt used to send the register and update requests
 * to the hash_gnode, backup_gnode and counter_gnode.
 * When the pkt is sent to a counter_gnode, a second `rip', which is the ip
 * of the hash_gnode who is contacting the counter_gnode, is appended at the
 * end of the pkt.
 */
struct andna_reg_pkt
{
	u_int	 	rip[MAX_IP_INT];	/* register_node ip */
 	u_int		hash[MAX_IP_INT];	/* md5 hash of the host name to
						   register. */
 	char		pubkey[ANDNA_PKEY_LEN];	/* public key of the register
 						   node. */
	u_short		hname_updates;		/* number of updates already 
						   made for the hostname */
	
 	char		sign[ANDNA_SIGNATURE_LEN]; /* RSA signature of the 
						      entire pkt (excluding sign
						      itself and flags */
	char 		flags;
	
} _PACKED_;
#define ANDNA_REG_PKT_SZ	     (sizeof(struct andna_reg_pkt))
#define ANDNA_REG_SIGNED_BLOCK_SZ (ANDNA_REG_PKT_SZ - ANDNA_SIGNATURE_LEN - \
				 	sizeof(char))

/*
 * The andna resolve request pkt is used to resolve hostnames and ips.
 */
struct andna_resolve_rq_pkt
{
	u_int	 	rip[MAX_IP_INT];	/* the ip of the requester node */
	char		flags;
	
	u_int           hash[MAX_IP_INT];       /* md5 hash of the hostname to
						   resolve. */
} _PACKED_;
#define ANDNA_RESOLVE_RQ_PKT_SZ		(sizeof(struct andna_resolve_rq_pkt))

/* 
 * The reply to the resolve request
 */
struct andna_resolve_reply_pkt
{
	u_int		ip[MAX_IP_INT];
	time_t		timestamp;		/* the last time the resolved 
						   hname was updated */
} _PACKED_;
#define ANDNA_RESOLVE_REPLY_PKT_SZ	(sizeof(struct andna_resolve_reply_pkt))

/* 
 * The reply to the reverse resolve request 
 */
struct andna_rev_resolve_reply_hdr
{
	u_char 		hostnames;	/* number of hostnames listed in the 
					   packed minus one */ 
} _PACKED_;
/*
 * The body of the reverse resolve reply is:
 * 	u_short		hname_sz[hdr.hostnames];
 * 	char		hostname1[ hname_sz[0] ];
 * 	char		hostname2[ hname_sz[1] ];
 * 	...			...
 */


int andna_load_caches(void);
int andna_save_caches(void);
void andna_init(void);

int andna_find_hash_gnode(int hash[MAX_IP_INT], inet_prefix *to);

int andna_register_hname(lcl_cache *alcl);
int andna_recv_reg_rq(PACKET rpkt);

int andna_check_counter(PACKET pkt);
int andna_recv_check_counter(PACKET rpkt);

int andna_resolve_hname(char *hname, inet_prefix *resolved_ip);
int andna_recv_resolve_rq(PACKET rpkt);

int andna_reverse_resolve(inet_prefix ip, char ***hostnames);
int andna_recv_rev_resolve_rq(PACKET rpkt);

andna_cache *get_andna_cache(inet_prefix to, int *counter);
int put_andna_cache(PACKET rq_pkt);

counter_c *get_counter_cache(inet_prefix to, int *counter);
int put_counter_cache(PACKET rq_pkt);

void *andna_hook(void *);
void andna_register_new_hnames(void);
void *andna_maintain_hnames_active(void *null);
int andna_main(void);
