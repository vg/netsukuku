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

#define ANDNA_MAX_BACKUP_GNODES		2
#define ANDNA_MAX_QUEUE			5
#define ANDNA_MAX_HNAME_LEN		512	/* (null terminator included) */
#define ANDNA_MAX_HOSTNAMES		256	/* Max number of hnames per node */
#define ANDNA_EXPIRATION_TIME		259200	/* 3 days (in seconds)*/
#define ANDNA_MIN_UPDATE_TIME		129600	/* The minum amount of time to
						   be waited before sending an 
						   update of the hname. */

#define ANDNA_PRIVKEY_BITS		1024
#define ANDNA_SKEY_LEN			608	/*packed in DER format*/
#define ANDNA_PKEY_LEN			140
#define ANDNA_HASH_SZ			(MAX_IP_SZ)
#define ANDNA_SIGNATURE_LEN		128

/* Returns the number of nodes to be used in a backup_gnode */
#define ANDNA_BACKUP_NODES(seeds)	({(seeds) > 8 ? 			\
					  ((seeds)*32)/MAXGROUPNODE : (seeds);})


/* 
 * * * Cache stuff * * *
 */

/* * andna_cache flags * */
#define ANDNA_BACKUP	1		/* We are a backup_node */
#define ANDNA_COUNTER	(1<<1)		/* We are a counter_node */
#define ANDNA_ROUNDED	(1<<2)		/* We are a rounded_hash_node */
#define ANDNA_FULL	(1<<3)		/* Queue full */

char andna_flags;
	
struct andna_cache_queue
{
	struct andna_cache_queue *next;
	struct andna_cache_queue *prev;
		
	inet_prefix	rip;			/* register_node ip */
	time_t		timestamp;
	u_short		hname_updates;		/* number of hname's updates */
	char		pubkey[ANDNA_PKEY_LEN];
};
typedef struct andna_cache_queue andna_cache_queue;
		
/*
 * This is the andna_cache, which keeps the entries of the registered
 * hostnames.
 */
struct andna_cache
{
	struct andna_cache *next;
	struct andna_cache *prev;
	
	u_int 		hash[MAX_IP_INT];	/* hostname's hash */
	char 		flags;

	u_short		queue_counter;
	andna_cache_queue *acq;			/* The queue of the registration.
						   The first is the active one */
};
typedef struct andna_cache andna_cache;


struct counter_c_hashes
{
	struct counter_c_hashes *next;
	struct counter_c_hashes *prev;

	time_t		timestamp;
	u_short		hname_updates;
	int		hash[MAX_IP_INT];
};
typedef struct counter_c_hashes counter_c_hashes;

/*
 * Counter node's cache
 */
struct counter_c
{
	struct counter_c *next;
	struct counter_c *prev;

	inet_prefix     rip;                    /* register_node ip */
	char            pubkey[ANDNA_PKEY_LEN];
	char		flags;
	
	u_short		hashes;			/* The number of hashes in cch */
	counter_c_hashes *cch;			/* The hashes of the hnames */
};
typedef struct counter_c counter_c;


/*
 * The local andna cache keeps the registered hostnames of the localhost and
 * its keys
 */
typedef struct lcl_cache_keyring
{
	char		privkey[ANDNA_SKEY_LEN];
	char		pubkey[ANDNA_PKEY_LEN];

	RSA		*priv_rsa;
} lcl_cache_keyring;

struct lcl_cache
{
	struct lcl_cache *next;
	struct lcl_cache *prev;
	
	char		*hostname;		/* The registered hostname */
	u_short		hname_updates;		/* How many updates we've done 
						   for this hostname */
	time_t          timestamp;		/* the last time when the hname
						   was updated. If it is 0, the
						   hname has still to be registered */
};
typedef struct lcl_cache lcl_cache;

/*
 * Global vars 
 */
andna_cache *andna_c;
int andna_c_counter;

counter_c *andna_counter_c;
int cc_counter;

lcl_cache_keyring lcl_keyring;
lcl_cache *andna_lcl;
int lcl_counter;

/*
 * * * ANDNA cache pack stuff * * * 
 */

/*
 * local cache pkt used to pack the entire local cache to save it in a file or
 * to send it to a node
 */
struct lcl_cache_pkt_hdr
{
	u_short		tot_caches;		/* How many lcl structs there 
						   are in the pkt's hdr */
 	char		privkey[ANDNA_SKEY_LEN];
 	char		pubkey[ANDNA_PKEY_LEN];
};
/* 
 * The body is:
 *	char		*hostname;
 *	u_short		hname_updates;
 *	time_t          timestamp;
 */
#define LCL_CACHE_PACK_SZ	(sizeof(u_short) + sizeof(time_t))

struct andna_cache_pkt_hdr
{
	u_short		tot_caches;
};
/*
 * The body is a struct andna_cache but andna_cache->acq in the struct is
 * substituted with the actual pack of the andna_cache_queue linked list.
 * There are a number of bodies equal to `tot_caches'.
 * So the complete pkt is:
 * 	struct andna_cache_pkt_hdr	hdr;
 * 	char 	acq[ANDNA_CACHE_QUEUE_PACK_SZ * hdr.tot_caches];
 */
#define ANDNA_CACHE_QUEUE_PACK_SZ	(sizeof(andna_cache_queue) - 		\
					(sizeof(andna_cache_queue *) * 2))
#define ANDNA_CACHE_BODY_PACK_SZ	(ANDNA_HASH_SZ + sizeof(char) +\
						sizeof(u_short))
#define ANDNA_CACHE_PACK_SZ(queue_counter)	((ANDNA_CACHE_QUEUE_PACK_SZ *	\
						(queue_counter)) +		\
						ANDNA_CACHE_BODY_PACK_SZ)

/*
 * The counter cache pkt is similar to the andna_cache_pkt, it is completely
 * arranged in the same way.
 */
struct counter_c_pkt_hdr
{
	u_short		tot_caches;
};
#define COUNTER_CACHE_HASHES_PACK_SZ	(sizeof(counter_c_hashes) - 		\
					(sizeof(counter_c_hashes *) * 2))
#define COUNTER_CACHE_BODY_PACK_SZ	(sizeof(counter_c) - 			\
					(sizeof(counter_c *) * 2) - 		\
					 sizeof(counter_c_hashes *))
#define COUNTER_CACHE_PACK_SZ(hashes)	((COUNTER_CACHE_HASHES_PACK_SZ*(hashes))\
					 + COUNTER_CACHE_BODY_PACK_SZ)

/*
 * * * Functions' declaration * * *
 */

void andna_caches_init(void);

void lcl_new_keyring(lcl_cache_keyring *keyring);
lcl_cache *lcl_cache_new(char *hname);
void lcl_cache_free(lcl_cache *alcl);
void lcl_cache_destroy(lcl_cache *head, int *counter);
lcl_cache *lcl_cache_find_hname(lcl_cache *head, char *hname);

andna_cache_queue *ac_queue_findpubk(andna_cache *ac, char *pubk);
andna_cache_queue *ac_queue_add(andna_cache *ac, inet_prefix rip, char *pubkey);
void ac_queue_del(andna_cache *ac, andna_cache_queue *acq);
void ac_queue_del_expired(andna_cache *ac);
andna_cache *andna_cache_findhash(int hash[MAX_IP_INT]);
andna_cache *andna_cache_addhash(int hash[MAX_IP_INT]);
void andna_cache_del_expired(void);

counter_c_hashes *cc_hashes_add(counter_c *cc, int hash[MAX_IP_INT]);
void cc_hashes_del(counter_c *cc, counter_c_hashes *cch);
void cc_hashes_del_expired(counter_c *cc);
counter_c_hashes *cc_findhash(counter_c *cc, int hash[MAX_IP_INT]);
counter_c *counter_c_findpubk(char *pubk);
counter_c *counter_c_add(inet_prefix *rip, char *pubkey);
void counter_c_del_expired(void);


char *pack_lcl_cache(lcl_cache_keyring *keyring, lcl_cache *local_cache, size_t *pack_sz);
lcl_cache *unpack_lcl_cache(lcl_cache_keyring *keyring, char *pack, size_t pack_sz, int *counter);

char *pack_andna_cache(andna_cache *acache, size_t *pack_sz);
andna_cache *unpack_andna_cache(char *pack, size_t pack_sz, int *counter);

char *pack_counter_cache(counter_c *countercache,  size_t *pack_sz);
counter_c *unpack_counter_cache(char *pack, size_t pack_sz, int *counter);

int save_lcl_cache(lcl_cache_keyring *keyring, lcl_cache *lcl, char *file);
lcl_cache *load_lcl_cache(lcl_cache_keyring *keyring, char *file, int *counter);

int save_andna_cache(andna_cache *acache, char *file);
andna_cache *load_andna_cache(char *file, int *counter);

int save_counter_c(counter_c *countercache, char *file);
counter_c *load_counter_c(char *file, int *counter);

int load_hostnames(char *file, lcl_cache **old_alcl_head, int *old_alcl_counter);
