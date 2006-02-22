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
 * andna_cache.c: 
 * Functions to manipulate all the andna's caches.
 */

#include "includes.h"

#include "llist.c"
#include "andna_cache.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"


int net_family;

void andna_caches_init(int family)
{
	net_family = family;

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
	memset(keyring, 0, sizeof(lcl_cache_keyring));
	
	if(!keyring->priv_rsa) {
		loginfo("Generating a new keyring for the future ANDNA requests.\n"
				"  The keyring will be saved in the lcl file");
		/* Generate the new key pair for the first time */
		keyring->priv_rsa = genrsa(ANDNA_PRIVKEY_BITS, &keyring->pubkey, 
				&keyring->pkey_len, &keyring->privkey, &keyring->skey_len);
	}
}

/*
 * lcl_destroy_keyring: destroys accurately the keyring ^_^ 
 */
void lcl_destroy_keyring(lcl_cache_keyring *keyring)
{
	if(keyring->priv_rsa)
		RSA_free(keyring->priv_rsa);
	if(keyring->pubkey)
		xfree(keyring->pubkey);
	if(keyring->privkey)
		xfree(keyring->privkey);
	
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
 * lcl_get_registered_hnames:
 * In `hostnames' is stored a pointer to a malloced array of pointers. Each
 * pointer points to a malloced hostname.
 * The hostnames stored in the array are taken from the `head' llist. Only
 * the hnames that have been registered are considered.
 * The number of hnames stored in `hostnames' is returned.
 */
int lcl_get_registered_hnames(lcl_cache *head, char ***hostnames)
{
	lcl_cache *alcl=head;
	int i=0, hname_sz;
	char **hnames;

	*hostnames=0;
	
	if(!alcl || !lcl_counter)
		return 0;
	
	hnames=xmalloc(lcl_counter * sizeof(char *));
	
	list_for(alcl) {
		if(!alcl->timestamp)
			continue;
		
		hname_sz=strlen(alcl->hostname)+1;
		hnames[i]=xmalloc(hname_sz);
		memcpy(hnames[i], alcl->hostname, hname_sz);
		i++;
	}

	*hostnames=hnames;
	return i;
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
	
	cur_t=time(0);

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
	
	memcpy(&acq->rip, rip.data, MAX_IP_SZ);

	if(ac->queue_counter >= ANDNA_MAX_QUEUE)
		ac->flags|=ANDNA_FULL;

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

/*
 * ac_queue_destroy: destroys an andna_cache_queue 
 */
void ac_queue_destroy(andna_cache *ac)
{
	andna_cache_queue *acq, *next;
	
	if(!ac || !ac->acq)
		return;

	acq=ac->acq;
	list_safe_for(acq, next)
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
 * andna_cache_destroy: destroys the andna_c llist 
 */
void andna_cache_destroy(void)
{
	andna_cache *ac=andna_c, *next;

        if(!andna_c_counter)
                return;

	list_safe_for(ac, next) {
		ac_queue_destroy(ac);
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

		clist_add(&cc->cch, &cc->hashes, cch);
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

void cc_hashes_destroy(counter_c *cc)
{
	counter_c_hashes *cch, *next;
	
	if(!cc || !cc->cch || !cc->hashes)
		return;

	cch=cc->cch;
	list_safe_for(cch, next)
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
 * counter_c_destroy: destroy the andna_counter_c llist
 */
void counter_c_destroy(void)
{
	counter_c *cc=andna_counter_c, *next;
	
	if(!cc)
		return;
	
	list_safe_for(cc, next) {
		cc_hashes_destroy(cc);
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
	memcpy(&rhc->ip, ip->data, MAX_IP_SZ);

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
	memcpy(&rhc->ip, ip->data, MAX_IP_SZ);

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
 * Note that the pack is in network byte order.
 */
char *pack_lcl_cache(lcl_cache_keyring *keyring, lcl_cache *local_cache, 
		size_t *pack_sz)
{
	struct lcl_cache_pkt_hdr lcl_hdr;
	lcl_cache *alcl=local_cache;
	int_info body_iinfo;
	size_t sz=0, slen;
	char *pack, *buf, *body;

	lcl_hdr.tot_caches=0;
	lcl_hdr.skey_len=keyring->skey_len;
	lcl_hdr.pkey_len=keyring->pkey_len;
	sz=LCL_CACHE_HDR_PACK_SZ(&lcl_hdr);
	
	/* Calculate the final pack size */
	list_for(alcl) {
		sz+=LCL_CACHE_BODY_PACK_SZ(strlen(alcl->hostname)+1);
		lcl_hdr.tot_caches++;
	}

	pack=buf=xmalloc(sz);
	memcpy(pack, &lcl_hdr, sizeof(struct lcl_cache_pkt_hdr));
	ints_host_to_network(pack, lcl_cache_pkt_hdr_iinfo);
	buf+=sizeof(struct lcl_cache_pkt_hdr);
		
	memcpy(buf, keyring->privkey, keyring->skey_len);
	buf+=keyring->skey_len;
	memcpy(buf, keyring->pubkey, keyring->pkey_len);
	buf+=keyring->pkey_len;
	
	*pack_sz=0;
	if(lcl_hdr.tot_caches) {
		int_info_copy(&body_iinfo, &lcl_cache_pkt_body_iinfo);
		
		alcl=local_cache;
		
		list_for(alcl) {
			body=buf;
			
			slen=strlen(alcl->hostname)+1;
			memcpy(buf, alcl->hostname, slen);

			buf+=slen;
			memcpy(buf, &alcl->hname_updates, sizeof(u_short));
			buf+=sizeof(u_short);
	
			memcpy(buf, &alcl->timestamp, sizeof(time_t));
			buf+=sizeof(time_t);

			body_iinfo.int_offset[0]=slen;
			body_iinfo.int_offset[1]=slen+sizeof(u_short);
			ints_host_to_network(body, body_iinfo);
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
 * Note `pack' is modified during the unpacking.
 */
lcl_cache *unpack_lcl_cache(lcl_cache_keyring *keyring, char *pack, size_t pack_sz, int *counter)
{
	struct lcl_cache_pkt_hdr *hdr;
	lcl_cache *alcl, *alcl_head=0;
	int_info body_iinfo;
	char *buf;
	u_char *pk;
	size_t slen, sz;
	int i=0;
		
	hdr=(struct lcl_cache_pkt_hdr *)pack;
	ints_network_to_host(hdr, lcl_cache_pkt_hdr_iinfo);

	if(hdr->tot_caches > ANDNA_MAX_HOSTNAMES)
		return 0;

	/*
	 * Restore the keyring 
	 */
	keyring->skey_len=hdr->skey_len;
	keyring->pkey_len=hdr->pkey_len;
	/* TODO: XXX: Check skey and pkey len */
	keyring->privkey=xmalloc(hdr->skey_len);
	keyring->pubkey=xmalloc(hdr->pkey_len);

	buf=pack+sizeof(struct lcl_cache_pkt_hdr);
	memcpy(keyring->privkey, buf, hdr->skey_len);
	buf+=hdr->skey_len;

	memcpy(keyring->pubkey, buf, hdr->pkey_len);
	buf+=hdr->pkey_len;
	
	pk=keyring->privkey;
	if(!(keyring->priv_rsa=get_rsa_priv((const u_char **)&pk,
					keyring->skey_len))) {
		error("Cannot unpack the priv key from the lcl_pack: %s",
				ssl_strerr());
		return 0;
	}

	*counter=0;
	if(hdr->tot_caches) {
		int_info_copy(&body_iinfo, &lcl_cache_pkt_body_iinfo);
		
		for(i=0, sz=0; i<hdr->tot_caches; i++) {
			slen=strlen(buf)+1;
			sz+=LCL_CACHE_BODY_PACK_SZ(slen);
			if(slen > ANDNA_MAX_HNAME_LEN || sz > pack_sz)
				goto finish;

			body_iinfo.int_offset[0]=slen;
			body_iinfo.int_offset[1]=slen+sizeof(u_short);
			ints_network_to_host(buf, body_iinfo);
			
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
 * The pack will be in network order.
 */
char *pack_andna_cache(andna_cache *acache, size_t *pack_sz)
{
	struct andna_cache_pkt_hdr hdr;
	andna_cache *ac=acache;
	andna_cache_queue *acq;
	char *pack, *buf, *p;
	size_t sz;
	time_t cur_t;
	u_int t;
	
	/* Calculate the pack size */
	hdr.tot_caches=0;
	sz=sizeof(struct andna_cache_pkt_hdr);
	list_for(ac) {
		sz+=ANDNA_CACHE_PACK_SZ(ac->queue_counter);
		hdr.tot_caches++;
	}
	
	pack=xmalloc(sz);
	memcpy(pack, &hdr, sizeof(struct andna_cache_pkt_hdr));
	ints_host_to_network(pack, andna_cache_pkt_hdr_iinfo);
	
	if(hdr.tot_caches) {
		cur_t=time(0);
		
		buf=pack + sizeof(struct andna_cache_pkt_hdr);
		ac=acache;
		list_for(ac) {
			p=buf;
			
			memcpy(buf, ac->hash, ANDNA_HASH_SZ);
			buf+=ANDNA_HASH_SZ;
			
			memcpy(buf, &ac->flags, sizeof(char));
			buf+=sizeof(char);

			memcpy(buf, &ac->queue_counter, sizeof(u_short));
			buf+=sizeof(u_short);
			
			ints_host_to_network(p, andna_cache_body_iinfo);

			acq=ac->acq;
			list_for(acq) {
				p=buf;
				
				memcpy(buf, acq->rip, MAX_IP_SZ);
				inet_htonl((u_int *)buf, net_family);
				buf+=MAX_IP_SZ;

				t = cur_t - acq->timestamp;
				memcpy(buf, &t, sizeof(uint32_t));
				buf+=sizeof(uint32_t);

				memcpy(buf, &acq->hname_updates, sizeof(u_short));
				buf+=sizeof(u_short);

				memcpy(buf, &acq->pubkey, ANDNA_PKEY_LEN);
				buf+=ANDNA_PKEY_LEN;
				
				ints_host_to_network(p, andna_cache_queue_body_iinfo);
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
 * Warning: `pack' will be modified during the unpacking.
 */
andna_cache *unpack_andna_cache(char *pack, size_t pack_sz, int *counter)
{
	struct andna_cache_pkt_hdr *hdr;
	andna_cache *ac, *ac_head=0;
	andna_cache_queue *acq;
	char *buf;
	size_t sz;
	int i, e, fake_int;
	time_t cur_t;

	hdr=(struct andna_cache_pkt_hdr *)pack;
	ints_network_to_host(hdr, andna_cache_pkt_hdr_iinfo);
	*counter=0;
	
	if(hdr->tot_caches) {
		cur_t=time(0);

		buf=pack + sizeof(struct andna_cache_pkt_hdr);
		sz=sizeof(struct andna_cache_pkt_hdr);
		
		for(i=0; i<hdr->tot_caches; i++) {
			sz+=ANDNA_CACHE_BODY_PACK_SZ;
			if(sz > pack_sz)
				goto finish; /* overflow */

			ac=xmalloc(sizeof(andna_cache));
			memset(ac, 0, sizeof(andna_cache));
			
			ints_network_to_host(buf, andna_cache_body_iinfo);
			
			memcpy(ac->hash, buf, ANDNA_HASH_SZ);
			buf+=ANDNA_HASH_SZ;
			
			memcpy(&ac->flags, buf, sizeof(char));
			buf+=sizeof(char);

			memcpy(&ac->queue_counter, buf, sizeof(u_short));
			buf+=sizeof(u_short);

			sz+=ANDNA_CACHE_QUEUE_PACK_SZ*ac->queue_counter;
			if(sz > pack_sz)
				goto finish; /* overflow */

			for(e=0; e < ac->queue_counter; e++) {
				acq=xmalloc(sizeof(andna_cache_queue));
				memset(acq, 0, sizeof(andna_cache_queue));
				
				ints_network_to_host(buf, andna_cache_queue_body_iinfo);
				
				memcpy(acq->rip, buf, MAX_IP_SZ);
				inet_ntohl(acq->rip, net_family);
				buf+=MAX_IP_SZ;

				acq->timestamp=0;
				acq->timestamp+=*(uint32_t *)buf;
				acq->timestamp = cur_t - acq->timestamp;
				buf+=sizeof(uint32_t);

				memcpy(&acq->hname_updates, buf, sizeof(u_short));
				buf+=sizeof(u_short);

				memcpy(&acq->pubkey, buf, ANDNA_PKEY_LEN);
				buf+=ANDNA_PKEY_LEN;

				clist_add(&ac->acq, &fake_int, acq);
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
 * The pack will be in network order.
 */
char *pack_counter_cache(counter_c *countercache, size_t *pack_sz)
{
	struct counter_c_pkt_hdr hdr;
	counter_c *cc=countercache;
	counter_c_hashes *cch;
	char *pack, *buf, *p;
	size_t sz;
	time_t cur_t;
	uint32_t t;
	
	/* Calculate the pack size */
	hdr.tot_caches=0;
	sz=sizeof(struct counter_c_pkt_hdr);
	list_for(cc) {
		sz+=COUNTER_CACHE_PACK_SZ(cc->hashes);
		hdr.tot_caches++;
	}
	
	pack=xmalloc(sz);
	memcpy(pack, &hdr, sizeof(struct counter_c_pkt_hdr));
	ints_host_to_network(pack, counter_c_pkt_hdr_iinfo);
	
	if(hdr.tot_caches) {
		cur_t=time(0);

		buf=pack + sizeof(struct counter_c_pkt_hdr);
		cc=countercache;
		list_for(cc) {
			p=buf;
		
			memcpy(buf, cc->pubkey, ANDNA_PKEY_LEN);
			buf+=ANDNA_PKEY_LEN;

			memcpy(buf, &cc->flags, sizeof(char));
			buf+=sizeof(char);

			memcpy(buf, &cc->hashes, sizeof(u_short));
			buf+=sizeof(u_short);

			ints_host_to_network(p, counter_c_body_iinfo);
			
			cch=cc->cch;
			list_for(cch) {
				p=buf;
				
				t = cur_t - cch->timestamp;
				memcpy(buf, &t, sizeof(uint32_t));
				buf+=sizeof(uint32_t);

				memcpy(buf, &cch->hname_updates, sizeof(u_short));
				buf+=sizeof(u_short);

				memcpy(buf, cch->hash, ANDNA_HASH_SZ);
				buf+=ANDNA_HASH_SZ;

				ints_host_to_network(p, counter_c_hashes_body_iinfo);
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
 * Note `pack' will be modified during the unpacking.
 */
counter_c *unpack_counter_cache(char *pack, size_t pack_sz, int *counter)
{
	struct counter_c_pkt_hdr *hdr;
	counter_c *cc, *cc_head=0;
	counter_c_hashes *cch;
	char *buf;
	size_t sz;
	int i, e, fake_int;
	time_t cur_t;

	hdr=(struct counter_c_pkt_hdr *)pack;
	ints_network_to_host(hdr, counter_c_pkt_hdr_iinfo);
	*counter=0;
	
	if(hdr->tot_caches) {
		cur_t = time(0);

		buf=pack + sizeof(struct counter_c_pkt_hdr);
		sz=sizeof(struct counter_c_pkt_hdr);
		
		for(i=0; i<hdr->tot_caches; i++) {
			sz+=COUNTER_CACHE_BODY_PACK_SZ;
			if(sz > pack_sz)
				goto finish; /* We don't want to overflow */

			cc=xmalloc(sizeof(counter_c));
			memset(cc, 0, sizeof(counter_c));
			
			ints_network_to_host(buf, counter_c_body_iinfo);
			
			memcpy(cc->pubkey, buf, ANDNA_PKEY_LEN);
			buf+=ANDNA_PKEY_LEN;

			memcpy(&cc->flags, buf, sizeof(char));
			buf+=sizeof(char);

			memcpy(&cc->hashes, buf, sizeof(u_short));
			buf+=sizeof(u_short);


			sz+=COUNTER_CACHE_HASHES_PACK_SZ * cc->hashes;
			if(sz > pack_sz)
				goto finish; /* bleah */
			
			for(e=0; e < cc->hashes; e++) {
				cch=xmalloc(sizeof(counter_c_hashes));
				memset(cch, 0, sizeof(counter_c_hashes));
				
				ints_network_to_host(buf, counter_c_hashes_body_iinfo);

				cch->timestamp=0;
				cch->timestamp+=*(uint32_t *)buf;
				cch->timestamp = cur_t - cch->timestamp;
				buf+=sizeof(uint32_t);

				memcpy(&cch->hname_updates, buf, sizeof(u_short));
				buf+=sizeof(u_short);

				memcpy(cch->hash, buf, ANDNA_HASH_SZ);
				buf+=ANDNA_HASH_SZ;

				clist_add(&cc->cch, &fake_int, cch);
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
 * The pack will be in network order.
 */
char *pack_rh_cache(rh_cache *rhcache, size_t *pack_sz)
{
	struct rh_cache_pkt_hdr rh_hdr;
	rh_cache *rhc=rhcache;
	int_info body_iinfo;
	size_t sz=0, slen;
	char *pack, *buf, *body;

	rh_hdr.tot_caches=0;
	sz=sizeof(struct rh_cache_pkt_hdr);
	
	/* Calculate the final pack size */
	list_for(rhc) {
		sz+=RH_CACHE_BODY_PACK_SZ(strlen(rhc->hostname)+1);
		rh_hdr.tot_caches++;
	}

	pack=xmalloc(sz);
	memcpy(pack, &rh_hdr, sizeof(struct rh_cache_pkt_hdr));
	ints_host_to_network(pack, rh_cache_pkt_hdr_iinfo);
	*pack_sz=0;

	if(rh_hdr.tot_caches) {
		buf=pack + sizeof(struct rh_cache_pkt_hdr);
		rhc=rhcache;
		
		list_for(rhc) {
			body=buf;

			slen=strlen(rhc->hostname)+1;
			memcpy(buf, rhc->hostname, slen);
			buf+=slen;

			memcpy(buf, &rhc->timestamp, sizeof(time_t));
			buf+=sizeof(time_t);
			
			memcpy(buf, rhc->ip, MAX_IP_SZ);
			buf+=MAX_IP_SZ;

			/* host -> network order */
			body_iinfo.int_offset[0]=slen;
			body_iinfo.int_offset[1]=slen+sizeof(time_t);
			ints_host_to_network(buf, body_iinfo);
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
 * Note `pack' will be modified during the unpacking.
 */
rh_cache *unpack_rh_cache(char *pack, size_t pack_sz, int *counter)
{
	struct rh_cache_pkt_hdr *hdr;
	rh_cache *rhc=0, *rhc_head=0;
	int_info body_iinfo;
	char *buf;
	size_t slen, sz;
	int i=0;
		
	hdr=(struct rh_cache_pkt_hdr *)pack;
	ints_network_to_host(hdr, rh_cache_pkt_hdr_iinfo);

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

			body_iinfo.int_offset[0]=slen;
			body_iinfo.int_offset[1]=slen+sizeof(time_t);
			ints_network_to_host(buf, body_iinfo);
			
			rhc=xmalloc(sizeof(rh_cache));
			memset(rhc, 0, sizeof(rh_cache));
			rhc->hostname=xstrdup(buf);
			rhc->hash=fnv_32_buf(rhc->hostname, 
					strlen(rhc->hostname), FNV1_32_INIT);
			buf+=slen;
			
			memcpy(&rhc->timestamp, buf, sizeof(time_t));
			buf+=sizeof(time_t);
			memcpy(rhc->ip, buf, MAX_IP_SZ);
			buf+=MAX_IP_SZ;

			clist_add(&rhc_head, counter, rhc);
		}
	}

finish:
	return rhc_head;
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
	lcl_cache *lcl=0;
	FILE *fd;
	char *pack=0;
	size_t pack_sz;
	
	if(!(fd=fopen(file, "r"))) {
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
		debug(DBG_NORMAL, "Malformed or empty lcl_cache file. "
				"Aborting load_lcl_cache().");
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
	andna_cache *acache=0;
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
		debug(DBG_NORMAL, "Malformed or empty andna_cache file."
				" Aborting load_andna_cache().");
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
	counter_c *countercache=0;
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
		debug(DBG_NORMAL, "Malformed or empty counter_c file. "
				"Aborting load_counter_c().");
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
	rh_cache *rh=0;
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
		debug(DBG_NORMAL, "Malformed or empty rh_cache file. "
				"Aborting load_rh_cache().");
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
	time_t cur_t, diff;
	int i=0;

	lcl_cache *alcl, *old_alcl, *new_alcl_head=0;
	int new_alcl_counter=0;

	if((fd=fopen(file, "r"))==NULL) {
		error("Cannot load any hostnames from %s: %s", file, strerror(errno));
		return -1;
	}

	cur_t=time(0);
	while(!feof(fd) && i < ANDNA_MAX_HOSTNAMES) {
		memset(buf, 0, ANDNA_MAX_HNAME_LEN+1);
		fgets(buf, ANDNA_MAX_HNAME_LEN, fd);
		if(feof(fd))
			break;

		if((*buf)=='#' || (*buf)=='\n' || !(*buf)) {
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
			 * If there is an equal entry in the old lcl_cache and
			 * it isn't expired, copy the old data in the new 
			 * struct.
			 */
			old_alcl = lcl_cache_find_hname(*old_alcl_head,
					alcl->hostname);
			if(old_alcl) {
				diff=cur_t - old_alcl->timestamp;
				if(diff < ANDNA_EXPIRATION_TIME) {
					alcl->timestamp=old_alcl->timestamp;
					alcl->hname_updates=old_alcl->hname_updates;
				}
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

/*
 * add_resolv_conf: It opens `file' and write in the first line `hname' moving
 * down the previous lines. The old `file' is backupped in `file'.bak.
 * Example: add_resolv_conf("nameserver 127.0.0.1", "/etc/resolv.conf").
 * Use del_resolv_conf to restore `file' with its backup.
 * On error -1 is returned.
 */
int add_resolv_conf(char *hname, char *file)
{
	FILE *fin=0,		/* `file' */
	     *fin_bak=0,	/* `file'.bak */
	     *fout=0,		/* The replaced `file' */
	     *fout_back=0;	/* The backup of `file' */
	     
	char *buf=0, *p, *file_bk=0;
	size_t buf_sz;
	int ret=0;

	/*
	 *  Open and read `file' 
	 */
	
	if(!(fin=fopen(file, "r"))) {
		error("add_resolv_conf: cannot load %s: %s", file, strerror(errno));
		ERROR_FINISH(ret, -1, finish);
	}

	/* Prepare the name of the backup file */
	file_bk=xmalloc(strlen(file) + strlen(".bak") + 1);
	*file_bk=0;
	strcpy(file_bk, file);
	strcat(file_bk, ".bak");
	
reread_fin:
	fseek(fin, 0, SEEK_END);
	buf_sz=ftell(fin);
	rewind(fin);
	
	buf=xmalloc(buf_sz);
	if(!fread(buf, buf_sz, 1, fin)) {
		error("add_resolv_conf: it wasn't possible to read the %s file",
				file);
		ERROR_FINISH(ret, -1, finish);
	}

	/* 
	 * If there is already the `hname' string in the first line, try to
	 * read `file'.bak, if it doesn't exist do nothing.
	 */
	if(buf_sz-1 >= strlen(hname) && !strncmp(buf, hname, strlen(hname))) {
		if(fin == fin_bak) {
			/*
			 * We've already read `fin_bak', and it has
			 * the `hname' string in its first line too. Stop it.
			 */
			goto finish;
		}
		
		debug(DBG_NORMAL, "add_resolv_conf: Reading %s instead", 
				file_bk);
		if(!(fin_bak=fopen(file_bk, "r")))
			goto finish;
		
		fclose(fin);
		fin=fin_bak;
		
		goto reread_fin;
	}
	
	/*
	 * Backup `file' in `file'.bak
	 */
	if(!(fout_back=fopen(file_bk, "w"))) {
		error("add_resolv_conf: cannot create a backup copy of %s in %s: %s", file,
			file_bk, strerror(errno));
		ERROR_FINISH(ret, -1, finish);
	}
	fwrite(buf, buf_sz, 1, fout_back);

	/*
	 * Delete `file'
	 */
	fclose(fin);
	fin=0;
	unlink(file);
	
	/*
	 * Add as a first line `hname' in `file'
	 */
	if(!(fout=fopen(file, "w"))) {
		error("add_resolv_conf: cannot reopen %s to overwrite it: %s", file, 
				strerror(errno));
		ERROR_FINISH(ret, -1, finish);
	}
	fprintf(fout, "%s\n", hname);
	p=buf;
	while(*p) {
		if(*p != '#')
			fprintf(fout, "#");
		while(*p) { 
			fprintf(fout, "%c", *p);
			if(*p == '\n')
				break;
			p++;
		}
		if(!*p)
			break;
		p++;
	}
	/*fwrite(buf, buf_sz, 1, fout);*/
	
finish:
	if(buf)
		xfree(buf);
	if(file_bk)
		xfree(file_bk);
	if(fin)
		fclose(fin);
	if(fout)
		fclose(fout);
	if(fout_back)
		fclose(fout_back);

	return ret;
}

/*
 * del_resolv_conf: restores the old `file' modified by add_resolv_conf() by 
 * copying `file'.bak over `file'. If the `hname' string is present in
 * `file'.bak it won't be written in `file'.
 * On error it returns -1.
 */
int del_resolv_conf(char *hname, char *file)
{
	FILE *fin=0, *fout=0;
	     
	char *buf=0, *file_bk=0, tmp_buf[128+1];
	size_t buf_sz;
	int ret=0;

	/*
	 *  Open and read `file'.bak 
	 */
	file_bk=xmalloc(strlen(file) + strlen(".bak") + 1);
	*file_bk=0;
	strcpy(file_bk, file);
	strcat(file_bk, ".bak");
	if(!(fin=fopen(file_bk, "r"))) {
		/*error("del_resolv_conf: cannot load %s: %s", file_bk, strerror(errno));*/
		ERROR_FINISH(ret, -1, finish);
	}

	fseek(fin, 0, SEEK_END);
	buf_sz=ftell(fin);
	rewind(fin);

	if(!buf_sz) {
		/* `file_bk' is empty, delete it */
		unlink(file_bk);
		ERROR_FINISH(ret, -1, finish);
	}
	
	buf=xmalloc(buf_sz);
	*buf=0;
	while(fgets(tmp_buf, 128, fin)) {
		/* Skip the line which is equal to `hname' */
		if(!strncmp(tmp_buf, hname, strlen(hname)))
			continue;
		strcat(buf, tmp_buf);
	}
	
	/*
	 * Delete `file'
	 */
	unlink(file);

	/*
	 * Copy `file'.bak in `file'
	 */
	
	if(!(fout=fopen(file, "w"))) {
		error("del_resolv_conf: cannot copy %s in %s: %s", file_bk,
			file, strerror(errno));
		ERROR_FINISH(ret, -1, finish);
	}
	fprintf(fout, "%s", buf);

	/*
	 * delete `file'.bak
	 */
	
	fclose(fin);
	fin=0;
	unlink(file_bk);
	
finish:
	if(buf)
		xfree(buf);
	if(file_bk)
		xfree(file_bk);
	if(fin)
		fclose(fin);
	if(fout)
		fclose(fout);

	return ret;
}
