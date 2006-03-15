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

#include "mark.h"
#include "err_errno.h"
#include "log.h"

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
	snprintf(ee->ip.outiface,IFNAMSIZ,"%s+",TUNNEL_IFACE);
	memset(ee->ip.outiface_mask,1,strlen(ee->ip.outiface));

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
	snprintf(ee->ip.outiface,IFNAMSIZ,"%s+",TUNNEL_IFACE);
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
//	struct in_addr not_inet_dst,not_inet_dst_mask;

	memset(rule,0,FILTER_RULE_SZ);
	e=(struct ipt_entry*)rule;
	et=(struct ipt_entry_target*)(rule+IPT_ENTRY_SZ);
	
	e->next_offset=FILTER_RULE_SZ;
	e->target_offset=IPT_ENTRY_SZ;
	memcpy(&(e->ip.dst),&not_inet_dst,sizeof(struct in_addr));
	memcpy(&(e->ip.dmsk),&not_inet_dst_mask,sizeof(struct in_addr));
	snprintf(e->ip.iniface,IFNAMSIZ,"%s+",TUNNEL_IFACE);
	memset(e->ip.iniface_mask,1,strlen(e->ip.iniface));
	e->ip.invflags=IPT_INV_DSTIP;

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
		debug(DBG_NORMAL,"New iptables chain ntk_mark_chain (mangle table) created.");
		debug(DBG_NORMAL,"-*- Don't touch this chain! -*-");
	}
	return 0;
dontwork:
	error("In ntk_mark_chain_init: -> %s", iptc_strerror(errno));
	err_ret(ERR_NETCHA,-1)
}
int mark_init()
{
	int res;
	iptc_handle_t t;
	char rule[RESTORE_OUTPUT_RULE_SZ]; /* the greater rule */
	int errs=0;

	res=inet_aton(NTK_NET_STR,&not_inet_dst);
	if (!res) {
		error("Strange error.");
		return -1;
	}
	res=inet_aton(NTK_NET_MASK_STR,&not_inet_dst_mask);
	if (!res) {
		error("Strange error.");
		return -1;
	}

	res=table_init(MANGLE_TABLE,&t);
	if (res) {
		error(err_str);
		error("Netfilter mangle table was not altered!");
		errs+=4;
	} else {
		res=ntk_mark_chain_init(&t);
		if (res) {
			error(err_str);
			error("Netfilter ntk_mark_chain was not created!");
			errs++;
		}
		restore_output_rule_init(rule);
		res=insert_rule(rule,&t,CHAIN_OUTPUT,0);
		if (res) {
			error(err_str);
			error("Netfilter restore-marking rule was not created!");
			errs++;
		}
		ntk_forward_rule_init(rule);
		res=insert_rule(rule,&t,CHAIN_POSTROUTING,0);
		if (res) {
			error(err_str);
			error("Netfilter restore-marking rule was not created!");
			errs++;
		}	
		igw_mark_rule_init(rule);
		res=insert_rule(rule,&t,CHAIN_FORWARD,0);
		if (res) {
			error(err_str);
			error("Netfilter igw death loop rule was not created!");
			errs+=1;
		}
		res=commit_rules(&t);
		if (res) {
			error(err_str);
			error("Netfilter mangle table was not altered!");
			errs=4;
		}
	}
	if (errs==0) {
		debug(DBG_NORMAL,"mark_init(): New chain ntk_mark_chain (mangle table) created.");
		debug(DBG_NORMAL,"mark_init(): Forwarding to ntk_mark_chain rule created.");
		debug(DBG_NORMAL,"mark_init(): Restoring mark rule created.");
		debug(DBG_NORMAL,"mark_init(): Igw death loop rule created.");
	}
	else 
		debug(DBG_NORMAL,"mark_init(),MANGLE_TABLE: %d (0-4) errors encountered.",errs);
	if (errs<4)
		debug(DBG_NORMAL,"-*- Don't touch these rules! -*-");

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
	return -errs;
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
		mark_rule_init(rule,TUNNEL_IFACE,i);
		res=append_rule(rule,&t,NTK_MARK_CHAIN);
		if (!res) {
			error(err_str);
			err_ret(ERR_NETRUL,-1);
		}
	}
	res=commit_rules(&t);
	if (!res) {
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
int mark_close()
{
	iptc_handle_t t;
	int res;
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
		res=delete_first_rule(&t,CHAIN_FORWARD);
		if (res) {
			error(err_str);
			error("Netfilter igw death loop FORWARD (mangle table) was not deleted!");
			errs++;
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
/*	res=table_init(FILTER_TABLE,&t);
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
	res=table_init(FILTER_TABLE,&t);*/
	return -errs;
}
