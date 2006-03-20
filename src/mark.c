	         /**************************************
	        *     AUTHOR: Federico Tomassini        *
	       *     Copyright (C) Federico Tomassini    *
	      *     Contact effetom@gmail.com             *
	     ***********************************************
	     *******          BEGIN 3/2006          ********
*************************************************************************
*                                              				* 
*  This program is free software; you can redistribute it and/or modify	*
*  it under the terms of the GNU General Public License as published by	*
*  the Free Software Foundation; either version 2 of the License, or	*
*  (at your option) any later version.					*
*									*
*  This program is distributed in the hope that it will be useful,	*
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	*
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*
*  GNU General Public License for more details.				*
*									*
************************************************************************/

/* 
 * This code is writed with my blood.
 * My hand was hurt. The keyboard was red.
 * In this code you can find my sacrifice.
 *
 * This code is a netfilter iptc library.
 * iptc is very bad documented: wisdom and 
 * debuggers was my friends to understand 
 * netfilter behavior. 
 * I hope you'll never need to code netfilter 
 * apps.
 * Memory dumpers are with you.
 */
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "iptunnel.h"
#include "mark.h"
#include "err_errno.h"
#include "log.h"

int death_loop_rule;
int clean_on_exit;
rule_store rr={0,0},fr={0,0},dr={0,0};

int table_init(const char *table, iptc_handle_t *t)
{
	*t=iptc_init(table);
	if (!(*t)) {
		error("In table_init, table %s: -> %s", table,iptc_strerror(errno));
		err_ret(ERR_NETFIL,-1);
	}
	return 0;

}
int insert_rule(const char *rule,iptc_handle_t *t,const char *chain,int pos)
{
	int res;
	res=iptc_insert_entry(chain,(struct ipt_entry*)rule,0,t);
	if (!res) {
		error("In insert_rule: %s.",iptc_strerror(errno));
		err_ret(ERR_NETRUL,-1);
	}
	return 0;
}
int append_rule(const char *rule,iptc_handle_t *t,const char *chain)
{
	int res;
	res=iptc_append_entry(chain,(struct ipt_entry*)rule,t);
	if (!res) {
		error("In append_rule: %s.",iptc_strerror(errno));
		err_ret(ERR_NETRUL,-1);
	}
	return 0;
}
int commit_rules(iptc_handle_t *t)
{
	int res;
	res=iptc_commit(t);
	if (!res) {
		error("In commit_rules: %s.",iptc_strerror(errno));
		err_ret(ERR_NETCOM,-1);
	}
	return 0;
}


/* Put in ,rule, the rule to be passed to the kernel.
 * ,rule, has to be RESTORE_OUTPUT_RULE_SZ-sized
 */
void restore_output_rule_init(unsigned char *rule)
{
	struct ipt_entry *ee;
	struct ipt_entry_match *em;
	struct ipt_entry_target *et;
	struct ipt_conntrack_info *ici;
	struct ipt_connmark_target_info *icmi;

	memset(rule,0,RESTORE_OUTPUT_RULE_SZ);
	
	ee=(struct ipt_entry*)(rule);
	em=(struct ipt_entry_match*)(rule+OFFSET_MATCH);
	ici=(struct ipt_conntrack_info*)(rule+OFFSET_MATCH_INFO);
	et=(struct ipt_entry_target*)(rule+OFFSET_TARGET);
	icmi=(struct ipt_connmark_target_info*)(rule+OFFSET_TARGET_INFO);

	ee->next_offset=RESTORE_OUTPUT_RULE_SZ;
	ee->target_offset=OFFSET_TARGET;
	
	snprintf(ee->ip.outiface,IFNAMSIZ,"%s+",NTK_TUNL_PREFIX);
	memset(ee->ip.outiface_mask,1,strlen(ee->ip.outiface)-1);

	strcpy(em->u.user.name,MOD_CONNTRACK);
	em->u.match_size=MATCH_SZ;;
	em->u.user.match_size=em->u.match_size;
	
	et->u.target_size=TARGET_SZ;
	et->u.user.target_size=et->u.target_size;
	strcpy(et->u.user.name,MOD_CONNMARK);

	ici->flags=1;
	ici->statemask|=IPT_CONNTRACK_STATE_BIT(IP_CT_RELATED);
	ici->statemask|=IPT_CONNTRACK_STATE_BIT(IP_CT_ESTABLISHED);

	icmi->mode=IPT_CONNMARK_RESTORE;
	icmi->mask= 0xffffffffUL;
}

void ntk_forward_rule_init(unsigned char *rule)
{
	struct ipt_entry *ee;
	struct ipt_entry_match *em;
	struct ipt_entry_target *et;
	struct ipt_conntrack_info *ici;
	
	memset(rule,0,NTK_FORWARD_RULE_SZ);
	
	ee=(struct ipt_entry*)(rule);
	em=(struct ipt_entry_match*)(rule+IPT_ENTRY_SZ);
	ici=(struct ipt_conntrack_info*)(rule+OFFSET_MATCH_INFO);
	et=(struct ipt_entry_target*)(rule+OFFSET_TARGET);

	ee->next_offset=NTK_FORWARD_RULE_SZ;
	ee->target_offset=OFFSET_TARGET;
	snprintf(ee->ip.outiface,IFNAMSIZ,"%s+",NTK_TUNL_PREFIX);
	memset(ee->ip.outiface_mask,1,strlen(ee->ip.outiface));

	strcpy(em->u.user.name,MOD_CONNTRACK);
	em->u.match_size=MATCH_SZ;
	em->u.user.match_size=em->u.match_size;

	ici->flags=1;
	ici->statemask|=IPT_CONNTRACK_STATE_BIT(IP_CT_NEW);

	et->u.target_size=IPT_ENTRY_TARGET_SZ+4;
	et->u.user.target_size=et->u.target_size;
	strcpy(et->u.user.name,NTK_MARK_CHAIN);
}
void mark_rule_init(unsigned char *rule,char *outiface,int outiface_num)
{
	struct ipt_entry *ee;
	struct ipt_entry_target *et;
	struct ipt_connmark_target_info *icmi;

	memset(rule,0,MARK_RULE_SZ);
	
	ee=(struct ipt_entry*)(rule);
	et=(struct ipt_entry_target*)(rule+IPT_ENTRY_SZ);
	icmi=(struct ipt_connmark_target_info*)(rule+IPT_ENTRY_SZ+IPT_ENTRY_TARGET_SZ);

	ee->next_offset=MARK_RULE_SZ;
	ee->target_offset=IPT_ENTRY_SZ;

	et->u.target_size=TARGET_SZ;
	et->u.user.target_size=et->u.target_size;
	strcpy(et->u.user.name,MOD_CONNMARK);

	icmi->mode=IPT_CONNMARK_SET;
	icmi->mask= 0xffffffffUL;
	snprintf(ee->ip.outiface,IFNAMSIZ,"%s%d",outiface,outiface_num);
	memset(ee->ip.outiface_mask,1,strlen(ee->ip.outiface));
	icmi->mark=outiface_num+1;
}
/*
int fill_mark_rule(unsigned char *rule,char *outiface,int outiface_num)
{
	struct ipt_entry *ee;
	struct ipt_connmark_target_info *icmi;

	if (outiface_num>MAX_MARK_RULES) {
		error("In fill_mark_rule: too many mark rules.");
		return -1;
	}
	ee=(struct ipt_entry*)rule;
	icmi=(struct ipt_connmark_target_info*)(rule+MARK_RULE_SZ-IPT_CM_TARGET_INFO_SZ);

	snprintf(ee->ip.outiface,IFNAMSIZ,"%s%d",outiface,outiface_num);
	memset(ee->ip.outiface_mask,1,strlen(ee->ip.outiface));
	icmi->mark=outiface_num+1;
	return 0;
}*/
void igw_mark_rule_init(char *rule)
{
	int res;
	struct ipt_entry *e;
	struct ipt_entry_target *et;
//	struct in_addr inet_dst,not_inet_dst_mask;

	memset(rule,0,FILTER_RULE_SZ);
	e=(struct ipt_entry*)rule;
	et=(struct ipt_entry_target*)(rule+IPT_ENTRY_SZ);
	
	e->next_offset=FILTER_RULE_SZ;
	e->target_offset=IPT_ENTRY_SZ;
//	memcpy(&(e->ip.dst),&inet_dst,sizeof(struct in_addr));
//	memcpy(&(e->ip.dmsk),&inet_dst_mask,sizeof(struct in_addr));
//	snprintf(e->ip.iniface,IFNAMSIZ,"%s+",DEFAULT_TUNL_PREFIX);
	snprintf(e->ip.iniface,IFNAMSIZ,"%s+",NTK_TUNL_PREFIX);
	memset(e->ip.iniface_mask,1,strlen(e->ip.iniface)-1);
//	e->ip.invflags=IPT_INV_DSTIP;

	et->u.target_size=IPT_ENTRY_TARGET_SZ+4;
	et->u.user.target_size=et->u.target_size;
	strcpy(et->u.user.name,MARK_TARGET);
	res=INET_MARK;
	memcpy(et->data,&res,4);
}

int ntk_mark_chain_init(iptc_handle_t *t)
{
	int res;
	res=iptc_is_chain(NTK_MARK_CHAIN,*t);
	if (res) {
		debug(DBG_NORMAL,"In mark_init: bizarre, ntk mangle" 
				 "chain is present yet. it will be flushed.");
		res=iptc_flush_entries(NTK_MARK_CHAIN,t);
		if (!res) 
			goto dontwork;
	} else {
		res=iptc_create_chain(NTK_MARK_CHAIN,t);
		if (!res) 
			goto dontwork;
	}
	return 0;
dontwork:
	error("In ntk_mark_chain_init: -> %s", iptc_strerror(errno));
	err_ret(ERR_NETCHA,-1)
}
int store_rules()
{
	int res;
	iptc_handle_t t;

	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETSTO,-1);
	}
	rr.e=(struct ipt_entry*)iptc_first_rule(CHAIN_OUTPUT,&t);
	fr.e=(struct ipt_entry*)iptc_first_rule(CHAIN_POSTROUTING,&t);
	/* Not elegant style, but faster */
	if (death_loop_rule) {
		dr.e=(struct ipt_entry*)iptc_first_rule(CHAIN_PREROUTING,&t);
		if (rr.e && fr.e && dr.e) {
			rr.sz=RESTORE_OUTPUT_RULE_SZ;
			rr.chain=CHAIN_OUTPUT;
			fr.sz=NTK_FORWARD_RULE_SZ;
			fr.chain=CHAIN_POSTROUTING;
			dr.sz=FILTER_RULE_SZ;
			dr.chain=CHAIN_FORWARD;
			commit_rules(&t);
			return 0;
		}
		else {
			commit_rules(&t);
			error("In store_rules: %s.",iptc_strerror(errno));
			err_ret(ERR_NETSTO,-1);
		}
	}
	if (rr.e && fr.e ) {
		rr.sz=RESTORE_OUTPUT_RULE_SZ;
		rr.chain=CHAIN_OUTPUT;
		fr.sz=NTK_FORWARD_RULE_SZ;
		fr.chain=CHAIN_POSTROUTING;
		commit_rules(&t);
		return 0;
	}
	commit_rules(&t);
	err_ret(ERR_NETSTO,-1);
}



	
int mark_init(int igw)
{
	int res;
	iptc_handle_t t;
	char rule[MAX_RULE_SZ];

	/*res=inet_aton(NTK_NET_STR,&inet_dst);
	if (!res) {
		error("Can not convert str to addr.");
		goto cannot_init;
	}
	res=inet_aton(NTK_NET_MASK_STR,&inet_dst_mask);
	if (!res) {
		error("Can not convert str to addr.");
		goto cannot_init;
	}*/

	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error(err_str);
		goto cannot_init;
	}
	res=ntk_mark_chain_init(&t);
	if (res) {
		error(err_str);
		error("Unable to create netfilter ntk_mark_chain.");
		goto cannot_init;
	}
	restore_output_rule_init(rule);
	res=insert_rule(rule,&t,CHAIN_OUTPUT,0);
	if (res) {
		error(err_str);
		error("Unable to create netfilter restore-marking rule.");
		goto cannot_init;
	}
	ntk_forward_rule_init(rule);
	res=insert_rule(rule,&t,CHAIN_POSTROUTING,0);
	if (res) {
		error(err_str);
		error("Unable to create netfilter forwarding rule.");
		goto cannot_init;
	}	
	if (igw) {
		death_loop_rule=1;
		igw_mark_rule_init(rule);
		res=insert_rule(rule,&t,CHAIN_PREROUTING,0);
		if (res) {
			error(err_str);
			error("Unable to create netfilter igw death loop rule.");
			death_loop_rule=0;
			goto cannot_init;
		}  
	}

	res=commit_rules(&t);
	if (res) {
		error(err_str);
		error("Netfilter mangle table was not altered!");
		goto cannot_init;
	}
	res=store_rules();
	if (res) {
		error(err_str);
		error("Rules storing failed: autocleaning netfilter on exit disable.");
		clean_on_exit=0;
	}
	clean_on_exit=1;
	debug(DBG_NORMAL,"Netfilter chain ntk_mark_chain created (mangle).");
	debug(DBG_NORMAL,"Netfilter restoring rule created (mangle->output).");
	debug(DBG_NORMAL,"Netfilter forwarding rule created (mangle->postrouting).");
	if (igw)
		debug(DBG_NORMAL,"Netfilter death loop igw rule created.");
	debug(DBG_NORMAL,"mark_init(), netfilter mangle table initialized.");
	debug(DBG_NORMAL,"-*- Don't touch netsukuku netfilter rules! -*-");
	return 0;
cannot_init:
	err_ret(ERR_MRKINI,-1);

/*	res=table_init(FILTER_TABLE,&t);
	if (res) {
		error(err_str);
		error("Netfilter filter table was not altered!");
		errss+=1;
	} else {
	
		res=commit_rules(&t);
		if (res) {
			error(err_str);
			error("Netfilter filter table was not modified!");
			errss=1;
		}
	}
	if (errss==0) {
		debug(DBG_NORMAL,"mark_init(): Marking igw conntction rule created.");
		debug(DBG_NORMAL,"-*- Don't touch this rule! -*-");
	} else
		debug(DBG_NORMAL,"mark_init(),FILTER_TABLE: %d (0-1) errors encountered.",errss);
	if (!errs && !errss)
		debug(DBG_NORMAL,"mark_init(): All's done.");*/
}
/* 
 * Count the number of rules in ntk_mangle_chain.
 *
 * Returns:
 * 	0
 * 	-1
 * 	nums
 */ 
int count_ntk_mark_chain(iptc_handle_t *t)
{
	int nchain=0;
	const struct ipt_entry *e;

	e=iptc_first_rule(NTK_MARK_CHAIN,t);
	while (e) {
		nchain++;
		e=iptc_next_rule(e,t);
	}
	return nchain;
}
int create_mark_rules(int n)
{
	int nchain;
	int res,i;
	char rule[MARK_RULE_SZ];
	iptc_handle_t t;

	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETRUL,-1);
	}
	nchain=count_ntk_mark_chain(&t);
	if (nchain==-1) {
		error("In create_mark_rules: can not read ntk_mark_chain.");
		err_ret(ERR_NETRUL,-1);
	} 
	if (nchain>=n) {
		debug(DBG_NORMAL,"In create_mark_rules: rules present yet.");
		return 0;
	}
	for (i=nchain;i<n;i++) {
		mark_rule_init(rule,NTK_TUNL_PREFIX,i);
		res=append_rule(rule,&t,NTK_MARK_CHAIN);
		if (res) {
			error(err_str);
			err_ret(ERR_NETRUL,-1);
		}
	}
	res=commit_rules(&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETRUL,-1);
	}
	debug(DBG_NORMAL,"Created %d marking rules.", n-nchain);
	return 0;
}

int delete_ntk_forward_chain(iptc_handle_t *t)
{	
	int res;

	res=iptc_is_chain(NTK_MARK_CHAIN,*t);
	if (!res)
		return 0;
	res=iptc_flush_entries(NTK_MARK_CHAIN,t);
        if (!res) 
		goto cannot_delete;
	res=iptc_delete_chain(NTK_MARK_CHAIN,t);
	if (!res) 
		goto cannot_delete;
	return 0;
        	
cannot_delete:	
	error("In delete_ntk_forward_chain: -> %s", iptc_strerror(errno));
	err_ret(ERR_NETDEL,-1);
}
int delete_first_rule(iptc_handle_t *t,const char *chain)
{
	int res;
	const struct ipt_entry *e;

	e=iptc_first_rule(chain,t);
	if (!e)
		return 0;
	res=iptc_delete_num_entry(chain,0,t);
	if (!res)
		goto cannot_delete;
	return 0;
cannot_delete:	
	error("In delete_first_rule: -> %s", iptc_strerror(errno));
	err_ret(ERR_NETDEL,-1);
}
int rule_position(rule_store *rule,iptc_handle_t *t)
{
	const struct ipt_entry *e;
	int res,count=-1,found=0;

	e=iptc_first_rule(rule->chain,t);
	while (e) {
		count++;
		res=memcmp(e,rule->e,rule->sz);
		if (!res) {
			found=1;
			break;
		}
		e=iptc_next_rule(e,t);
	}
	return found?count:-1;
}
int delete_rule(rule_store *rule,iptc_handle_t *t)
{
	int pos,res;
	pos=rule_position(rule,t);
	if (pos==-1) {
		debug(DBG_NORMAL,"No rule in %s to be deleted.",rule->chain);
		return 0;
	}
	res=iptc_delete_num_entry(rule->chain,pos,t);
	if (!res) {
		debug(DBG_NORMAL,"Unable to delete rule in chain %s.",rule->chain);
		err_ret(ERR_NETDEL,-1);
	}
	return 0;
}
		
int mark_close()
{
	iptc_handle_t t;
	int res;

	if (!clean_on_exit) {
		debug(DBG_NORMAL,"mark_close: cleaning is not my task.");
		return 0;
	}
	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETDEL,-1);
	}
	res=0;
	res+=delete_rule(&rr,&t);
	res+=delete_rule(&fr,&t);
	if (death_loop_rule)
		res+=delete_rule(&dr,&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETRST,-1);
	}
	res=delete_ntk_forward_chain(&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETRST,-1);
	}
	res=commit_rules(&t);
	if (res) {
		error(err_str);
		err_ret(ERR_NETRST,-1);
	}
	debug(DBG_NORMAL,"Netfilter completely restored.");
	return 0;
}


















	/*
	int errs=0;

	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error("Netfilter mangle chain is not loadable: nothing will be restored.");
		errs+=4;
	} else {
		res=delete_first_rule(&t,CHAIN_POSTROUTING);
		if (res) {
			error(err_str);
			error("Netfilter ntk-rule on POSTROUTING (mangle table) was not deleted!");
			errs++;
		}
		res=delete_first_rule(&t,CHAIN_OUTPUT);
		if (res) {
			error(err_str);
			error("Netfilter ntk-rule on OUTPUT (mangle table) was not deleted!");
			errs++;
		}

		if(death_loop_rule) {
			res=delete_first_rule(&t,CHAIN_FORWARD);
			if (res) {
				error(err_str);
				error("Netfilter igw death loop FORWARD (mangle table) "
						"was not deleted!");
				errs++;
			}
		}

		res=delete_ntk_forward_chain(&t);
		if (res) {
			error(err_str);
			error("Netfilter ntk-chain on mangle table was not flushed and removed!");
			errs++;
		}
		res=commit_rules(&t);
		if (res) {
			error(err_str);
			error("Error committing rules: netfilter mangle table was not restored!");
			errs++;
		}
	}
	debug(DBG_NORMAL,"Netfilter mangle table restored with %d errors (0-4).",errs);
	res=table_init(FILTER_TABLE,&t);
	if (!res) {
		error(err_str);
		error("Netfilter filter chain is not loadable: nothing will be restored.");
		errss++;
	} else {
		res=delete_first_rule(&t,CHAIN_FORWARD);
		if (!res) {
			error(err_str);
			error("Netfilter ntk-rule on FORWARD (mangle table) was not deleted!");
			errss++;
		}
	}
	debug(DBG_NORMAL,"Netfilter filter table restored with %d errors (0-1).",errss);
	res=table_init(FILTER_TABLE,&t);
	return -errs;
}*/
