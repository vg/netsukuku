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
 *
 * --
 * andna_cache.c: 
 * Functions to manipulate all the andna's caches.
 */

#include "includes.h"

#include "llist.c"
#include "inet.h"
#include "crypto.h"
#include "andna_cache.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"


void andna_caches_init(void)
{
	memset(&lcl_keyring, 0, sizeof(lcl_cache_keyring));

	andna_lcl=(lcl_cache *)clist_init(&lcl_counter);
	andna_c=(andna_cache *)clist_init(&andna_c_counter);
	andna_counter_c=(counter_c *)clist_init(&cc_counter);
	andna_rhc=(rh_cache *)clist_init(&rhc_counter);
}

/*
 *  *  *  Local Cache functions  *  *  *
 */

/*
 * lcl_cache_new_keyring: Generates a new keyring for the local cache if there
 * aren't any.
 */
void lcl_new_keyring(lcl_cache_keyring *keyring)
{
	u_char *priv_dump, *pub_dump;

	if(!keyring->priv_rsa) {
		loginfo("Generating a new keyring for the future ANDNA requests.\n"
				"  The keyring will be saved in the lcl file");
		/* Generate the new key pair for the first time */
		keyring->priv_rsa = genrsa(ANDNA_PRIVKEY_BITS, &pub_dump, 0, 
				&priv_dump, 0);
		memcpy(keyring->privkey, priv_dump, ANDNA_SKEY_LEN);
		memcpy(keyring->pubkey, pub_dump, ANDNA_PKEY_LEN);

		xfree(priv_dump);
		xfree(pub_dump);
	}
}
/*
 * lcl_destroy_keyring: destroys accurately the keyring ^_^ 
 */
void lcl_destroy_keyring(lcl_cache_keyring *keyring)
{
	if(keyring->priv_rsa)
		RSA_free(keyring->priv_rsa);
	memset(keyring, 0, sizeof(lcl_cache_keyring));
}

/*
 * lcl_cache_new: builds a new lcl_cache generating a new rsa key pair and
 * setting the hostname in the struct 
 */
lcl_cache *lcl_cache_new(char *hname)
{
	lcl_cache *alcl;
	
	alcl=(lcl_cache *)xmalloc(sizeof(lcl_cache));
	memset(alcl, 0, sizeof(lcl_cache));

	alcl->hostname = xstrdup(hname);
	alcl->hash = fnv_32_buf(hname, strlen(hname), FNV1_32_INIT);

	return alcl;
}

void lcl_cache_free(lcl_cache *alcl) 
{
	if(alcl->hostname)
		xfree(alcl->hostname);
}

void lcl_cache_destroy(lcl_cache *head, int *counter)
{
	lcl_cache *alcl=head, *next;
	
	if(!alcl || !lcl_counter)
		return;
	
	list_safe_for(alcl, next) {
		lcl_cache_free(alcl);
		xfree(alcl);
	}
	*counter=0;
}

lcl_cache *lcl_cache_find_hname(lcl_cache *head, char *hname)
{
	lcl_cache *alcl=head;
	u_int hash;
	
	if(!alcl || !lcl_counter)
		return 0;

	hash = fnv_32_buf(hname, strlen(hname), FNV1_32_INIT);
	list_for(alcl)
		if(alcl->hash == hash && alcl->hostname && 
			!strncmp(alcl->hostname, hname, ANDNA_MAX_HNAME_LEN))
			return alcl;
	return 0;
}


/*
 *  *  *  Andna Cache functions  *  *  *
 */

andna_cache_queue *ac_queue_findpubk(andna_cache *ac, char *pubk)
{
	andna_cache_queue *acq=ac->acq;
	
	if(!acq)
		return 0;
	list_for(acq)
		if(!memcmp(acq->pubkey, pubk, ANDNA_PKEY_LEN))
				return acq;
	return 0;
}

/*
 * ac_queue_add: adds a new entry in the andna cache queue, which is
 * `ac'->acq. The elements in the new `ac'->acq are set to `rip' and `pubkey'.
 * If an `ac'->acq struct with an `ac'->acq->pubkey equal to `pubkey' already
 * exists, then only the ac->acq->timestamp and the ac->acq->rip will be updated.
 * It returns the pointer to the acq struct. If it isn't possible to add a new
 * entry in the queue, 0 will be returned.
 */
andna_cache_queue *ac_queue_add(andna_cache *ac, inet_prefix rip, char *pubkey)
{
	andna_cache_queue *acq;
	time_t cur_t;
	int update=0;
	
	/* 
	 * This call is not necessary because it's already done by
	 * andna_cache_del_expired().
	 * * ac_queue_del_expired(ac); * * 
	 */
	
	if(!(acq=ac_queue_findpubk(ac, pubkey))) {
		if(ac->queue_counter >= ANDNA_MAX_QUEUE || ac->flags & ANDNA_FULL)
			return 0;

		acq=xmalloc(sizeof(andna_cache_queue));
		memset(acq, 0, sizeof(andna_cache_queue));
		
		memcpy(acq->pubkey, pubkey, ANDNA_PKEY_LEN);
		clist_add(&ac->acq, &ac->queue_counter, acq);
	} else
		update=1;
	
	memcpy(&acq->rip, &rip, sizeof(inet_prefix));

	if(ac->queue_counter >= ANDNA_MAX_QUEUE)
		ac->flags|=ANDNA_FULL;

	cur_t=time(0);
	if(update && cur_t > acq->timestamp && 
			(cur_t - acq->timestamp) < ANDNA_MIN_UPDATE_TIME) {
		/* 
		 * The request to update the hname was sent too early. 
		 * Ignore it.
		 */
		do_nothing();
	} else
		acq->timestamp=cur_t;

	return acq;
}

void ac_queue_del(andna_cache *ac, andna_cache_queue *acq)
{
	clist_del(&ac->acq, &ac->queue_counter, acq);
	ac->flags&=~ANDNA_FULL;
}

/*
 * ac_queue_del_expired: removes the expired entries from the
 * andna_cache_queue `ac'->acq.
 */
void ac_queue_del_expired(andna_cache *ac)
{
	andna_cache_queue *acq, *next;
	time_t cur_t;
	
	if(!ac || !ac->acq)
		return;

	cur_t=time(0);
	acq=ac->acq;
	list_safe_for(acq, next)
		if(cur_t - acq->timestamp > ANDNA_EXPIRATION_TIME)
			ac_queue_del(ac, acq);
}

andna_cache *andna_cache_findhash(int hash[MAX_IP_INT])
{
	andna_cache *ac=andna_c;

	if(!andna_c_counter)
		return 0;

	list_for(ac)
		if(!memcmp(ac->hash, hash, ANDNA_HASH_SZ))
			return ac;
	return 0;
}

andna_cache *andna_cache_addhash(int hash[MAX_IP_INT])
{
	andna_cache *ac;

	andna_cache_del_expired();
	
	if(!(ac=andna_cache_findhash(hash))) {
		ac=xmalloc(sizeof(andna_cache));
		memset(ac, 0, sizeof(andna_cache));
		memcpy(ac->hash, hash, ANDNA_HASH_SZ);

		clist_add(&andna_c, &andna_c_counter, ac);
	}

	return ac;
}

void andna_cache_del_expired(void)
{
        andna_cache *ac=andna_c, *next;

        if(!andna_c_counter)
                return;

	list_safe_for(ac, next) {
		ac_queue_del_expired(ac);
		if(!ac->queue_counter)
			clist_del(&andna_c, &andna_c_counter, ac);
	}
}



/*
 *  *  *  Counter Cache functions  *  *  *
 */

counter_c_hashes *cc_hashes_add(counter_c *cc, int hash[MAX_IP_INT])
{
	counter_c_hashes *cch;

	/* The purge is already done in counter_c_del_expired(), so it is not
	 * necessary to call it here.
	 * * cc_hashes_del_expired(cc); * *
	 */

	if(!(cch=cc_findhash(cc, hash))) {
		if(cc->hashes >= ANDNA_MAX_HOSTNAMES || cc->flags & ANDNA_FULL)
			return 0;
		
		cch=xmalloc(sizeof(counter_c_hashes));
		memset(cch, 0, sizeof(counter_c_hashes));
		memcpy(cch->hash, hash, ANDNA_HASH_SZ);
	}
	
	cch->timestamp=time(0);
	
	if(cc->hashes >= ANDNA_MAX_HOSTNAMES)
		cc->flags|=ANDNA_FULL;
	
	return cch;
}

void cc_hashes_del(counter_c *cc, counter_c_hashes *cch)
{
	clist_del(&cc->cch, &cc->hashes, cch);
	cc->flags&=~ANDNA_FULL;
}

void cc_hashes_del_expired(counter_c *cc)
{
	counter_c_hashes *cch, *next;
	time_t cur_t;
	
	if(!cc || !cc->cch || !cc->hashes)
		return;
	
	cur_t=time(0);
	cch=cc->cch;

	list_safe_for(cch, next)
		if(cur_t - cch->timestamp > ANDNA_EXPIRATION_TIME)
			cc_hashes_del(cc, cch);
}

counter_c_hashes *cc_findhash(counter_c *cc, int hash[MAX_IP_INT])
{
	counter_c_hashes *cch=cc->cch;

	if(!cc->hashes || !cch)
		return 0;
	
	list_for(cch)
		if(!memcmp(cch->hash, hash, ANDNA_HASH_SZ))
			return cch;
	return 0;
}

counter_c *counter_c_findpubk(char *pubk)
{
	counter_c *cc=andna_counter_c;
	
	if(!cc_counter || !cc)
		return 0;

	list_for(cc)
		if(!memcmp(&cc->pubkey, pubk, ANDNA_PKEY_LEN))
			return cc;
	return 0;
}

counter_c *counter_c_add(inet_prefix *rip, char *pubkey)
{
	counter_c *cc;

	counter_c_del_expired();

	if(!(cc=counter_c_findpubk(pubkey))) {
		cc=xmalloc(sizeof(counter_c));
		memset(cc, 0, sizeof(counter_c));

		memcpy(cc->pubkey, pubkey, ANDNA_PKEY_LEN);
		clist_add(&andna_counter_c, &cc_counter, cc);
	}

	memcpy(&cc->rip, rip, sizeof(inet_prefix));

	return cc;
}

void counter_c_del_expired(void)
{
	counter_c *cc=andna_counter_c, *next;
	
	if(!cc)
		return;
	
	list_safe_for(cc, next) {
		cc_hashes_del_expired(cc);
		if(!cc->hashes)
			clist_del(&andna_counter_c, &cc_counter, cc);
	}
}


/*
 *  *  * Resolved hostnames cache functions  *  *  *
 */

rh_cache *rh_cache_new(char *hname, time_t timestamp, inet_prefix *ip)
{
	rh_cache *rhc;
	
	rhc=xmalloc(sizeof(rh_cache));
	memset(rhc, 0, sizeof(rh_cache));
	
	rhc->hash=fnv_32_buf(hname, strlen(hname), FNV1_32_INIT);
	rhc->hostname=xstrdup(hname);
	rhc->timestamp=timestamp;
	memcpy(&rhc->ip, ip, sizeof(inet_prefix));

	return rhc;
}

rh_cache *rh_cache_add(char *hname, time_t timestamp, inet_prefix *ip)
{
	rh_cache *rhc;

	if(!(rhc=rh_cache_find_hname(hname))) {
		if(rhc_counter >= ANDNA_MAX_HOSTNAMES) {
			/* Delete the oldest struct in cache */
			rhc=andna_rhc;
			clist_del(&andna_rhc, &rhc_counter, rhc);
		}

		rhc=rh_cache_new(hname, timestamp, ip);
		clist_add(&andna_rhc, &rhc_counter, rhc);
	}

	rhc->timestamp=timestamp;
	memcpy(&rhc->ip, ip, sizeof(inet_prefix));

	return rhc;
}

rh_cache *rh_cache_find_hname(char *hname)
{
	rh_cache *rhc=andna_rhc;
	u_int hash;

	if(!rhc || !rhc_counter)
		return 0;
	
	hash=fnv_32_buf(hname, strlen(hname), FNV1_32_INIT);
	
	list_for(rhc)
		if(rhc->hash == hash && !strncmp(hname, rhc->hostname,
						ANDNA_MAX_HNAME_LEN))
			return rhc;
	return 0;
}

void rh_cache_del(rh_cache *rhc)
{
	if(rhc->hostname)
		xfree(rhc->hostname);
	clist_del(&andna_rhc, &rhc_counter, rhc);
}

void rh_cache_del_expired(void)
{
	rh_cache *rhc=andna_rhc, *next;
	time_t cur_t;

	if(!rhc || !rhc_counter)
		return;

	cur_t=time(0);
	
	list_safe_for(rhc, next)
		if(cur_t - rhc->timestamp > ANDNA_EXPIRATION_TIME)
			rh_cache_del(rhc);
}

void rh_cache_flush(void)
{
	rh_cache *rhc=andna_rhc, *next;

	list_safe_for(rhc, next)
		rh_cache_del(rhc);
}

/*
 *  *  *  Pack/Unpack functions  *  *  *
 */

/*
 * pack_lcl_cache: packs the entire local cache linked list that starts with
 * the head `local_cache'. The size of the pack is stored in `pack_sz'.
 * The given `keyring' is packed too.
 * The pointer to the newly allocated pack is returned.
 */
char *pack_lcl_cache(lcl_cache_keyring *keyring, lcl_cache *local_cache, 
		size_t *pack_sz)
{
	struct lcl_cache_pkt_hdr lcl_hdr;
	lcl_cache *alcl=local_cache;
	size_t sz=0, slen;
	char *pack, *buf;

	lcl_hdr.tot_caches=0;
	memcpy(lcl_hdr.privkey, keyring->privkey, ANDNA_SKEY_LEN);
	memcpy(lcl_hdr.pubkey, keyring->pubkey, ANDNA_PKEY_LEN);
	sz=sizeof(struct lcl_cache_pkt_hdr);
	
	/* Calculate the final pack size */
	list_for(alcl) {
		sz+=LCL_CACHE_BODY_PACK_SZ(strlen(alcl->hostname)+1);
		lcl_hdr.tot_caches++;
	}

	pack=xmalloc(sz);
	memcpy(pack, &lcl_hdr, sizeof(struct lcl_cache_pkt_hdr));
	*pack_sz=0;

	if(lcl_hdr.tot_caches) {
		buf=pack + sizeof(struct lcl_cache_pkt_hdr);
		alcl=local_cache;
		
		list_for(alcl) {
			slen=strlen(alcl->hostname+1);
			memcpy(buf, alcl->hostname, slen);

			buf+=slen;
			memcpy(buf, &alcl->hname_updates, sizeof(u_short));
			buf+=sizeof(u_short);
	
			memcpy(buf, &alcl->timestamp, sizeof(time_t));
			buf+=sizeof(time_t);
		}
	}

	*pack_sz=sz;
	return pack;
}

/*
 * unpack_lcl_cache: unpacks a packed local cache linked list and returns its head.
 * In `keyring' it restores the packed keys. 
 * `counter' is set to the number of struct in the llist.
 * On error 0 is returned.
 */
lcl_cache *unpack_lcl_cache(lcl_cache_keyring *keyring, char *pack, size_t pack_sz, int *counter)
{
	struct lcl_cache_pkt_hdr *hdr;
	lcl_cache *alcl, *alcl_head=0;
	char *buf;
	u_char *pk;
	size_t slen, sz;
	int i=0;
		
	hdr=(struct lcl_cache_pkt_hdr *)pack;
	if(hdr->tot_caches > ANDNA_MAX_HOSTNAMES)
		return 0;

	/*
	 * Restore the keyring 
	 */
	memcpy(keyring->privkey, hdr->privkey, ANDNA_SKEY_LEN);
	memcpy(keyring->pubkey, hdr->pubkey, ANDNA_PKEY_LEN);
	
	pk=keyring->privkey;
	if(!(keyring->priv_rsa=get_rsa_priv((const u_char **)&pk, ANDNA_SKEY_LEN))) {
		error("Cannot unpack the priv key from the lcl_pack: %s",
				ssl_strerr());
		return 0;
	}

	*counter=0;
	if(hdr->tot_caches) {
		buf=pack + sizeof(struct lcl_cache_pkt_hdr);

		for(i=0, sz=0; i<hdr->tot_caches; i++) {
			slen=strlen(buf)+1;
			sz+=LCL_CACHE_BODY_PACK_SZ(slen);
			if(slen > ANDNA_MAX_HNAME_LEN || sz > pack_sz)
				goto finish;
			
			alcl=xmalloc(sizeof(lcl_cache));
			memset(alcl, 0, sizeof(lcl_cache));
			alcl->hostname=xstrdup(buf);
			alcl->hash=fnv_32_buf(alcl->hostname, 
					strlen(alcl->hostname), FNV1_32_INIT);
			buf+=slen;
			
			memcpy(&alcl->hname_updates, buf,  sizeof(u_short));
			buf+=sizeof(u_short);

			memcpy(&alcl->timestamp, buf, sizeof(time_t));
			buf+=sizeof(time_t);

			clist_add(&alcl_head, counter, alcl);
		}
	}

finish:
	return alcl_head;
}

/*
 * pack_andna_cache: packs the entire andna cache linked list that starts with
 * the head `acache'. The size of the pack is stored in `pack_sz'.
 * The pointer to the newly allocated pack is returned.
 */
char *pack_andna_cache(andna_cache *acache, size_t *pack_sz)
{
	struct andna_cache_pkt_hdr hdr;
	andna_cache *ac=acache;
	andna_cache_queue *acq;
	char *pack, *buf, *p;
	size_t sz;
	
	/* Calculate the pack size */
	hdr.tot_caches=0;
	sz=sizeof(struct andna_cache_pkt_hdr);
	list_for(ac) {
		sz+=ANDNA_CACHE_PACK_SZ(ac->queue_counter);
		hdr.tot_caches++;
	}
	
	pack=xmalloc(sz);
	memcpy(pack, &hdr, sizeof(struct andna_cache_pkt_hdr));
	
	if(hdr.tot_caches) {
		buf=pack + sizeof(struct andna_cache_pkt_hdr);
		ac=acache;
		list_for(ac) {
			p=(char *)ac + (sizeof(andna_cache *) * 2);
			memcpy(buf, p, ANDNA_CACHE_BODY_PACK_SZ);
			buf+=ANDNA_CACHE_BODY_PACK_SZ;

			acq=ac->acq;
			list_for(acq) {
				p=(char *)acq + (sizeof(andna_cache_queue *) * 2);
				memcpy(buf, p, ANDNA_CACHE_QUEUE_PACK_SZ);
				buf+=ANDNA_CACHE_QUEUE_PACK_SZ;
			}
		}
	}

	*pack_sz=sz;
	return pack;
}


/*
 * unpack_andna_cache: unpacks a packed andna cache linked list and returns the
 * its head.  `counter' is set to the number of struct in the llist.
 * On error 0 is returned.
 */
andna_cache *unpack_andna_cache(char *pack, size_t pack_sz, int *counter)
{
	struct andna_cache_pkt_hdr *hdr;
	andna_cache *ac, *ac_head=0;
	andna_cache_queue *acq;
	char *buf, *p;
	size_t sz;
	int i, e;

	hdr=(struct andna_cache_pkt_hdr *)pack;
	*counter=0;
	
	if(hdr->tot_caches) {
		buf=pack + sizeof(struct andna_cache_pkt_hdr);
		
		for(i=0, sz=0; i<hdr->tot_caches; i++) {
			ac=(andna_cache *)buf;
			sz+=ANDNA_CACHE_PACK_SZ(ac->queue_counter);
			if(sz > pack_sz)
				goto finish;

			ac=xmalloc(sizeof(andna_cache));
			memset(ac, 0, sizeof(andna_cache));
			
			p=(char *)ac + (sizeof(andna_cache *) * 2);
			memcpy(p, buf, ANDNA_CACHE_BODY_PACK_SZ);
			buf+=ANDNA_CACHE_BODY_PACK_SZ;

			for(e=0; e < ac->queue_counter; e++) {
				acq=xmalloc(sizeof(andna_cache_queue));
				memset(acq, 0, sizeof(andna_cache_queue));
				
				p=(char *)acq + (sizeof(andna_cache_queue *) * 2);
				memcpy(p, buf, ANDNA_CACHE_QUEUE_PACK_SZ);
				buf+=ANDNA_CACHE_QUEUE_PACK_SZ;

				clist_add(&ac->acq, &ac->queue_counter, acq);
			}

			clist_add(&ac_head, counter, ac);
		}
	}
finish:
	return ac_head;
}

/*
 * pack_counter_cache: packs the entire counter cache linked list that starts 
 * with the head `counter'. The size of the pack is stored in `pack_sz'.
 * The pointer to the newly allocated pack is returned.
 */
char *pack_counter_cache(counter_c *countercache, size_t *pack_sz)
{
	struct counter_c_pkt_hdr hdr;
	counter_c *cc=countercache;
	counter_c_hashes *cch;
	char *pack, *buf, *p;
	size_t sz;
	
	/* Calculate the pack size */
	hdr.tot_caches=0;
	sz=sizeof(struct counter_c_pkt_hdr);
	list_for(cc) {
		sz+=COUNTER_CACHE_PACK_SZ(cc->hashes);
		hdr.tot_caches++;
	}
	
	pack=xmalloc(sz);
	memcpy(pack, &hdr, sizeof(struct counter_c_pkt_hdr));
	
	if(hdr.tot_caches) {
		buf=pack + sizeof(struct counter_c_pkt_hdr);
		cc=countercache;
		list_for(cc) {
			p=(char *)cc + (sizeof(counter_c *) * 2);
			memcpy(buf, p, COUNTER_CACHE_BODY_PACK_SZ);
			buf+=COUNTER_CACHE_BODY_PACK_SZ;

			cch=cc->cch;
			list_for(cch) {
				p=(char *)cch + (sizeof(counter_c_hashes *) * 2);
				memcpy(buf, p, COUNTER_CACHE_HASHES_PACK_SZ);
				buf+=COUNTER_CACHE_HASHES_PACK_SZ;
			}
		}
	}

	*pack_sz=sz;
	return pack;
}


/*
 * unpack_counter_cache: unpacks a packed counter cache linked list and returns the
 * its head.  `counter' is set to the number of struct in the llist.
 * On error 0 is returned.
 */
counter_c *unpack_counter_cache(char *pack, size_t pack_sz, int *counter)
{
	struct counter_c_pkt_hdr *hdr;
	counter_c *cc, *cc_head=0;
	counter_c_hashes *cch;
	char *buf, *p;
	size_t sz;
	int i, e;

	hdr=(struct counter_c_pkt_hdr *)pack;
	*counter=0;
	
	if(hdr->tot_caches) {
		buf=pack + sizeof(struct counter_c_pkt_hdr);
		
		for(i=0, sz=0; i<hdr->tot_caches; i++) {
			cc=(counter_c *)buf;
			sz+=COUNTER_CACHE_PACK_SZ(cc->hashes);
			if(sz > pack_sz)
				goto finish;

			cc=xmalloc(sizeof(counter_c));
			memset(cc, 0, sizeof(counter_c));
			
			p=(char *)cc + (sizeof(counter_c *) * 2);
			memcpy(p, buf, COUNTER_CACHE_BODY_PACK_SZ);
			buf+=COUNTER_CACHE_BODY_PACK_SZ;

			for(e=0; e < cc->hashes; e++) {
				cch=xmalloc(sizeof(counter_c_hashes));
				memset(cch, 0, sizeof(counter_c_hashes));
				
				p=(char *)cch + (sizeof(counter_c_hashes *) * 2);
				memcpy(p, buf, COUNTER_CACHE_HASHES_PACK_SZ);
				buf+=COUNTER_CACHE_HASHES_PACK_SZ;

				clist_add(&cc->cch, &cc->hashes, cch);
			}

			clist_add(&cc_head, counter, cc);
		}
	}
finish:
	return cc_head;
}


/*
 * pack_rh_cache: packs the entire resolved hnames cache linked list that starts 
 * with the head `rhcache'. The size of the pack is stored in `pack_sz'.
 * The pointer to the newly allocated pack is returned.
 */
char *pack_rh_cache(rh_cache *rhcache, size_t *pack_sz)
{
	struct rh_cache_pkt_hdr rh_hdr;
	rh_cache *rhc=rhcache;
	size_t sz=0, slen;
	char *pack, *buf;

	rh_hdr.tot_caches=0;
	sz=sizeof(struct rh_cache_pkt_hdr);
	
	/* Calculate the final pack size */
	list_for(rhc) {
		sz+=RH_CACHE_BODY_PACK_SZ(strlen(rhc->hostname)+1);
		rh_hdr.tot_caches++;
	}

	pack=xmalloc(sz);
	memcpy(pack, &rh_hdr, sizeof(struct rh_cache_pkt_hdr));
	*pack_sz=0;

	if(rh_hdr.tot_caches) {
		buf=pack + sizeof(struct rh_cache_pkt_hdr);
		rhc=rhcache;
		
		list_for(rhc) {
			slen=strlen(rhc->hostname)+1;
			memcpy(buf, rhc->hostname, slen);
			buf+=slen;

			memcpy(buf, &rhc->timestamp, sizeof(time_t));
			buf+=sizeof(time_t);
			
			memcpy(buf, &rhc->ip, sizeof(inet_prefix));
			buf+=sizeof(inet_prefix);
		}
	}

	*pack_sz=sz;
	return pack;
}

/*
 * unpack_rh_cache: unpacks a packed resolved hnames cache linked list and 
 * returns its head.
 * `counter' is set to the number of struct in the llist.
 * On error 0 is returned.
 */
rh_cache *unpack_rh_cache(char *pack, size_t pack_sz, int *counter)
{
	struct rh_cache_pkt_hdr *hdr;
	rh_cache *rhc, *rhc_head=0;
	char *buf;
	size_t slen, sz;
	int i=0;
		
	hdr=(struct rh_cache_pkt_hdr *)pack;
	if(hdr->tot_caches > ANDNA_MAX_RHC_HNAMES)
		return 0;

	*counter=0;
	if(hdr->tot_caches) {
		buf=pack + sizeof(struct rh_cache_pkt_hdr);

		for(i=0, sz=0; i<hdr->tot_caches; i++) {
			slen=strlen(buf)+1;
			sz+=RH_CACHE_BODY_PACK_SZ(slen);
			if(slen > ANDNA_MAX_HNAME_LEN || sz > pack_sz)
				goto finish;
			
			rhc=xmalloc(sizeof(rh_cache));
			memset(rhc, 0, sizeof(rh_cache));
			rhc->hostname=xstrdup(buf);
			rhc->hash=fnv_32_buf(rhc->hostname, 
					strlen(rhc->hostname), FNV1_32_INIT);
			buf+=slen;
			
			memcpy(&rhc->timestamp, buf, sizeof(time_t));
			buf+=sizeof(time_t);
			memcpy(&rhc->ip, buf, sizeof(inet_prefix));
			buf+=sizeof(inet_prefix);


			clist_add(&rhc_head, counter, rhc);
		}
	}

finish:
	return rhc;
}


/*
 *  *  *  Save/Load functions  *  *  *
 */

/*
 * save_lcl_cache: saves a local cache linked list and the relative keyring in 
 * the `file' specified.
 */
int save_lcl_cache(lcl_cache_keyring *keyring, lcl_cache *lcl, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;

	/*Pack!*/
	pack=pack_lcl_cache(keyring, lcl, &pack_sz);
	if(!pack_sz || !pack)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the lcl_cache in %s: %s", file, strerror(errno));
		return -1;
	}

	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);
	
	xfree(pack);
	fclose(fd);
	return 0;
}

/*
 * load_lcl_cache: loads from `file' a local cache list and returns the head
 * of the newly allocated llist. In `counter' it is stored the number of
 * structs of the llist, and in `keyring' it restores the RSA keys.
 * On error 0 is returned.
 */
lcl_cache *load_lcl_cache(lcl_cache_keyring *keyring, char *file, int *counter)
{
	lcl_cache *lcl;
	FILE *fd;
	char *pack=0;
	size_t pack_sz;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the lcl_cache from %s: %s", file, strerror(errno));
		return 0;
	}

	fseek(fd, 0, SEEK_END);
	pack_sz=ftell(fd);
	rewind(fd);
	
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto finish;
	
	lcl=unpack_lcl_cache(keyring, pack, pack_sz, counter);

finish:
	if(pack)
		xfree(pack);
	fclose(fd);
	if(!lcl)
		error("Malformed lcl_cache file. Aborting load_lcl_cache().");
	return lcl;
}


/*
 * save_andna_cache: saves an andna cache linked list in the `file' specified 
 */
int save_andna_cache(andna_cache *acache, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;

	/*Pack!*/
	pack=pack_andna_cache(acache, &pack_sz);
	if(!pack_sz || !pack)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the andna_cache in %s: %s", file, strerror(errno));
		return -1;
	}

	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);
	
	xfree(pack);
	fclose(fd);
	return 0;
}

/*
 * load_andna_cache: loads from `file' an andna cache list and returns the head
 * of the newly allocated llist. In `counter' it is stored the number of
 * list's structs.
 * On error 0 is returned.
 */
andna_cache *load_andna_cache(char *file, int *counter)
{
	andna_cache *acache;
	FILE *fd;
	char *pack=0;
	size_t pack_sz;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the andna_cache from %s: %s", file, strerror(errno));
		return 0;
	}

	fseek(fd, 0, SEEK_END);
	pack_sz=ftell(fd);
	rewind(fd);
	
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto finish;
	
	acache=unpack_andna_cache(pack, pack_sz, counter);

finish:
	if(pack)
		xfree(pack);
	fclose(fd);
	if(!acache)
		error("Malformed andna_cache file. Aborting load_andna_cache().");
	return acache;
}


/*
 * save_counter_c: saves a counter cache linked list in the `file' specified 
 */
int save_counter_c(counter_c *countercache, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;

	/*Pack!*/
	pack=pack_counter_cache(countercache, &pack_sz);
	if(!pack_sz || !pack)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the counter_c in %s: %s", file, strerror(errno));
		return -1;
	}

	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);
	
	xfree(pack);
	fclose(fd);
	return 0;
}

/*
 * load_counter_c: loads from `file' a counter cache list and returns the head
 * of the newly allocated llist. In `counter' it is stored the number of
 * list's structs.
 * On error 0 is returned.
 */
counter_c *load_counter_c(char *file, int *counter)
{
	counter_c *countercache;
	FILE *fd;
	char *pack=0;
	size_t pack_sz;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the counter_c from %s: %s", file, strerror(errno));
		return 0;
	}

	fseek(fd, 0, SEEK_END);
	pack_sz=ftell(fd);
	rewind(fd);
	
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto finish;
	
	countercache=unpack_counter_cache(pack, pack_sz, counter);

finish:
	if(pack)
		xfree(pack);
	fclose(fd);
	if(!countercache)
		error("Malformed counter_c file. Aborting load_counter_c().");
	return countercache;
}


/*
 * save_rh_cache: saves the resolved hnames cache linked list `rh' in the
 * `file' specified.
 */
int save_rh_cache(rh_cache *rh, char *file)
{
	FILE *fd;
	size_t pack_sz;
	char *pack;

	/*Pack!*/
	pack=pack_rh_cache(rh, &pack_sz);
	if(!pack_sz || !pack)
		return 0;
	
	if((fd=fopen(file, "w"))==NULL) {
		error("Cannot save the rh_cache in %s: %s", file, strerror(errno));
		return -1;
	}

	/*Write!*/
	fwrite(pack, pack_sz, 1, fd);
	
	xfree(pack);
	fclose(fd);
	return 0;
}

/*
 * load_rh_cache: loads from `file' a resolved hnames cache list and returns 
 * the head of the newly allocated llist. In `counter' it is stored the number
 * of structs of the llist.
 * On error 0 is returned.
 */
rh_cache *load_rh_cache(char *file, int *counter)
{
	rh_cache *rh;
	FILE *fd;
	char *pack=0;
	size_t pack_sz;
	
	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load the rh_cache from %s: %s", file, strerror(errno));
		return 0;
	}

	fseek(fd, 0, SEEK_END);
	pack_sz=ftell(fd);
	rewind(fd);
	
	pack=xmalloc(pack_sz);
	if(!fread(pack, pack_sz, 1, fd))
		goto finish;
	
	rh=unpack_rh_cache(pack, pack_sz, counter);

finish:
	if(pack)
		xfree(pack);
	fclose(fd);
	if(!rh)
		error("Malformed rh_cache file. Aborting load_rh_cache().");
	return rh;
}


/*
 * load_hostnames: reads the `file' specified and reads each line in it.
 * The strings read are the hostnames that will be registered in andna.
 * Only ANDNA_MAX_HOSTNAMES lines are read. Each line can be maximum of
 * ANDNA_MAX_HNAME_LEN character long.
 * This function updates automagically the old local cache that is pointed by 
 * `*old_alcl_head'. The hostnames that are no more present in the loaded
 * `file' are discarded from the local cache.
 * Since a new local cache is allocated and the old is destroyed, the new
 * pointer to it is written in `**old_alcl_head'.
 * The `old_alcl_counter' is updated too.
 * This function shall be used each time the `file' changes.
 * On error -1 is returned, otherwise 0 shall be the sacred value.
 */
int load_hostnames(char *file, lcl_cache **old_alcl_head, int *old_alcl_counter)
{
	FILE *fd;
	char buf[ANDNA_MAX_HNAME_LEN+1];
	size_t slen;
	int i=0;

	lcl_cache *alcl, *old_alcl, *new_alcl_head=0;
	int new_alcl_counter=0;

	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load any hostnames from %s: %s", file, strerror(errno));
		return -1;
	}

	while(!feof(fd) && i < ANDNA_MAX_HOSTNAMES) {
		memset(buf, 0, ANDNA_MAX_HNAME_LEN+1);
		fgets(buf, ANDNA_MAX_HNAME_LEN, fd);
		if(feof(fd))
			break;

		if(*buf=='#' || *buf=='\n') {
			/* Strip off the comment lines */
			continue;
		} else {
			slen=strlen(buf);
			if(buf[slen-1] == '\n') {
				/* Don't include the newline in the string */
				buf[slen-1]='\0';
				slen=strlen(buf);
			}

			/* Add the hname in the new local cache */
			alcl = lcl_cache_new(buf);
			clist_add(&new_alcl_head, &new_alcl_counter, alcl);

			/*
			 * If there is an equal entry in the old lcl_cache,
			 * copy the old data in the new struct.
			 */
			old_alcl = lcl_cache_find_hname(*old_alcl_head,
					alcl->hostname);
			if(old_alcl) {
				alcl->timestamp=old_alcl->timestamp;
				alcl->hname_updates=old_alcl->hname_updates;
			}
			i++;
		}
	}

	/* Remove completely the old lcl_cache */
	lcl_cache_destroy(*old_alcl_head, old_alcl_counter);

	/* Update the pointers */
	*old_alcl_head=new_alcl_head;
	*old_alcl_counter=new_alcl_counter;

	return 0;
}
