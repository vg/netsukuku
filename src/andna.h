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
 */

/* How many different andna pkt can be flooded simultaneusly */
#define ANDNA_MAX_FLOODS	(ANDNA_MAX_QUEUE*3) 
/* How many new hash_gnodes are supported in the andna hash_gnode mutation */
#define ANDNA_MAX_NEW_GNODES	1024

/* These arrays keeps the latest reg_pkt and counter_check IDs to drop pkts
 * alreay received during the floods. These arrays are actually a FIFO, so the
 * last pkt_id will be always at the 0 position, while the first one will be
 * at the last position */
int last_reg_pkt_id[ANDNA_MAX_FLOODS];
int last_counter_pkt_id[ANDNA_MAX_FLOODS];
int last_spread_acache_pkt_id[ANDNA_MAX_FLOODS];

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

/* 
 * The single_acache pkt is used to get from an old hash_gnode a single
 * andna_cache, which has the wanted `hash'. Its propagation method is similar
 * to that of andna_resolve_rq_pkt, but each new hash_gnode, which receives
 * the pkt, adds in the body pkt its ip. The added ips are used as excluded 
 * hash_gnode by find_hash_gnode(). In this way each time an old hash_gnode 
 * receives the pkt, can verify if it is, at that current time, the true old 
 * hash_gnode by excluding the hash_gnodes listed in the pkt body. If it 
 * notices that there's an hash_gnode older than it, it will append its ip in 
 * the pkt body and will forward it to that older hash_gnode. And so on, until
 * the pkt reaches a true old hash_gnode, or cannot be forwarded anymore since
 * there are no more older hash_gnodes.
 */
struct single_acache_hdr
{
	u_int		rip[MAX_IP_INT];	/* the ip of the requester node */
	u_int		hash[MAX_IP_INT];
	u_short		hgnodes;		/* Number of hgnodes in the 
						   body. */
	u_char		flags;
} _PACKED_;
/*
 * The single_acache body is:
 * struct {
 * 	u_int		hgnode[MAX_IP_INT];
 * } body[new_hash_gnode_hdr.hgnodes];
 */
#define SINGLE_ACACHE_PKT_SZ(hgnodes)	(sizeof(struct single_acache_hdr)+\
						MAX_IP_SZ*(hgnodes))
/*
 * The single_acache_reply is just an andna_cache_pkt with a single cache.
 */


/*
 * Tell the node, which receives the pkt, to send a ANDNA_GET_SINGLE_ACACHE
 * request to fetch the andna_cache for the `hash' included in the pkt.
 */
struct spread_acache_pkt
{
	u_int		hash[MAX_IP_INT];
} _PACKED_;
#define SPREAD_ACACHE_PKT_SZ	(sizeof(struct spread_acache_pkt))


int andna_load_caches(void);
int andna_save_caches(void);
void andna_init(void);

int andna_register_hname(lcl_cache *alcl);
int andna_recv_reg_rq(PACKET rpkt);

int andna_check_counter(PACKET pkt);
int andna_recv_check_counter(PACKET rpkt);

int andna_resolve_hname(char *hname, inet_prefix *resolved_ip);
int andna_recv_resolve_rq(PACKET rpkt);

int andna_reverse_resolve(inet_prefix ip, char ***hostnames);
int andna_recv_rev_resolve_rq(PACKET rpkt);

int spread_single_acache(u_int hash[MAX_IP_INT]);
int recv_spread_single_acache(PACKET rpkt);

andna_cache *get_single_andna_c(u_int hash[MAX_IP_INT],	u_int hash_gnode[MAX_IP_INT]);
int put_single_acache(PACKET rpkt);

andna_cache *get_andna_cache(inet_prefix to, int *counter);
int put_andna_cache(PACKET rq_pkt);

counter_c *get_counter_cache(inet_prefix to, int *counter);
int put_counter_cache(PACKET rq_pkt);

void *andna_hook(void *);
void andna_register_new_hnames(void);
void *andna_maintain_hnames_active(void *null);
void *andna_main(void *);
