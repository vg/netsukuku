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
#include "mark.h"
#include "err_errno.h"
#include "log.h"

int mgl_table_init()
{
	mgl_table=iptc_init(MANGLE_TABLE);
	if (!mgl_table) {
		error("In mgl_table_init: -> %s", iptc_strerror(errno));
		err_ret(ERR_NETFIL,-1);
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
/* Create and commit the rule which will mark outgoing connections.
 * returns:
 * 	0
 * 	-1
 */
int output_rule_commit()
{
	char rule[RESTORE_OUTPUT_RULE_SZ];
	int res;

	restore_output_rule_init(rule);
	res=maybe_rule_present(rule,CHAIN_OUTPUT,RESTORE_OUTPUT_RULE_SZ);
	if (res==-1) {
		error("In output_rule_commit(): can not controls the first output rule.");
		iptc_commit(&mgl_table);
		return -1;
	}
	if (res) {
		debug(DBG_NORMAL,"Netfilter output rule is present yet.");
		return 0;
	}
	res=mgl_table_init();
	if (res) {
		error(err_str);
		return -1;
	}
	res=iptc_insert_entry(CHAIN_OUTPUT,(struct ipt_entry*)rule,0,&mgl_table);
	if (!res) {
		iptc_commit(&mgl_table);
		goto problems;
	}
	res=iptc_commit(&mgl_table);
	if (!res) 
		goto problems;
	debug(DBG_NORMAL,"Netfilter output rule created.");
	return 0;
problems:
	error("In output_rule_commit(): %s.", iptc_strerror(errno));
	return -1;
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

int ntk_forward_rule_commit()
{
	char rule[NTK_FORWARD_RULE_SZ];
	int res;

	ntk_forward_rule_init(rule);
	res=maybe_rule_present(rule,CHAIN_POSTROUTING,NTK_FORWARD_RULE_SZ);
	if (res==-1) {
		error("In ntk_forward_rule_commit(): can not controls the first output rule.");
		iptc_commit(&mgl_table);
		return -1;
	}
	if (res) {
		debug(DBG_NORMAL,"Netfilter ntk forward rule is present yet.");
		return 0;
	}
	res=mgl_table_init();
	if (res) 
		goto problems;
	res=iptc_insert_entry(CHAIN_POSTROUTING,(struct ipt_entry*)rule,0,&mgl_table);
	if (!res) {
		iptc_commit(&mgl_table);
		goto problems;
	}
	res=iptc_commit(&mgl_table);
	if (!res) 
		goto problems;
	debug(DBG_NORMAL,"Netfilter forward rule created.");
	return 0;
problems:
	error("In output_rule_commit(): %s.", iptc_strerror(errno));
	return -1;
}
int maybe_rule_present(unsigned char *rule,const char *chain,int rule_sz)
{
	const struct ipt_entry *e;
	unsigned char *crow;
	int res;
	res=mgl_table_init();
	if (res) {
		error("In maybe_rule_present: can not init mangle table.");
		return -1;
	}
	e=iptc_first_rule(chain,&mgl_table);
	if (!e) 
		res=1;
	else {
		crow=(unsigned char*)e;
		res=memcmp(crow,rule,sizeof(struct ipt_ip));
	}
	iptc_commit(&mgl_table);
	if (!res) 
		return 1;
	return 0;
}

void mark_rule_init(unsigned char *rule)
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
}
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
}
int create_mark_rules(int n)
{
	int nchain;
	int res,i;
	char rule[MARK_RULE_SZ];
	nchain=count_ntk_mark_chain();
	if (nchain==-1) {
		error("In create_mark_rules: can not read ntk_mark_chain.");
		err_ret(ERR_NETRUL,-1);
	} 
/*	if (nchain==0) {
		loginfo("This is the first mark rule: now creating restore-marking and forward rules.");
		res=output_rule_commit();
		if (res) {
			//error("In create_mark_rules: can not create restore-marking rule.");
			printf("In create_mark_rules: can not create restore-marking rule.");
			return -1;
		}
		res=ntk_forward_rule_commit();
		if (res) {
			//error("In create_mark_rules: can not create ntk-forward rule.");
			printf("In create_mark_rules: can not create ntk-forward rule.");
			return -1;
		}
	}*/
	if (nchain>=n) {
		debug(DBG_NORMAL,"In create_mark_rules: rules present yet.");
		return 0;
	}
	res=mgl_table_init();
	if (res) {
		error("In create_mark_rules: can not init mangle table.");
		err_ret(ERR_NETRUL,-1);
	}
	mark_rule_init(rule);
	for (i=nchain;i<n;i++) {
		fill_mark_rule(rule,TUNNEL_IFACE,i);
		res=iptc_append_entry(NTK_MARK_CHAIN,(struct ipt_entry*)rule,&mgl_table);
		if (!res) {
			error("In create_mark_rules: can not append rule -> %s",iptc_strerror(errno));
			err_ret(ERR_NETRUL,-1);
		}
	}
	res=iptc_commit(&mgl_table);
	if (!res) {
		error("In create_mark_rules: can not commit rules: %s",iptc_strerror(errno));
		err_ret(ERR_NETRUL,-1);
	}
	debug(DBG_NORMAL,"Created %d marking rules.", n-nchain);
	return 0;
}
		
	
/* 
 * Create a new mangle chain, which name is NTK_MARK_CHAIN 
 * or, if found, flushes his entries
 * Create output and forward rule also.
 * Returns:
 * 	0 if OK
 * 	-1 on error
 */
int mark_init(void)
{
	int res;

	res=mgl_table_init();
	if (res) {
		//printf(err_str);
		//error(err_str);
		goto cannot_init;
	}
	res=iptc_is_chain(NTK_MARK_CHAIN,mgl_table);
	if (res) {
		debug(DBG_NORMAL,"In mark_init: bizarre, ntk mangle chain is present yet. it will be flushed.");
		res=iptc_flush_entries(NTK_MARK_CHAIN,&mgl_table);
		if (!res) {
			error("In mark_init: -> %s", iptc_strerror(errno));
			goto cannot_init;
		}
	} else {
		res=iptc_create_chain(NTK_MARK_CHAIN,&mgl_table);
		if (!res) {
			error("In mark_init: -> %s", iptc_strerror(errno));
			goto cannot_init;
		}
		loginfo("New iptables chain ntk_mark_chain (mangle table) created.");
		loginfo("-*- Don't touch this chain! -*-");
	}
	res=iptc_commit(&mgl_table);
	if (!res) {
		error("In mark_init: can not commit.");
		goto cannot_init;
	}
	res=output_rule_commit();
	if (res==-1) {
		error("In mark_init: can not create output rule.");
		goto cannot_init;
	}
	res=ntk_forward_rule_commit();
	if (res==-1) {
		error("In mark_init: can not create forward rule.");
		goto cannot_init;
	}
	return 0;

cannot_init:
	err_ret(ERR_MRKINI,-1);
}

/* 
 * Count the number of rules in ntk_mangle_chain.
 *
 * Returns:
 * 	0
 * 	-1
 * 	nums
 */ 
int count_ntk_mark_chain()
{
	int res,nchain=0;
	const struct ipt_entry *e;
	res=mgl_table_init();
	if (res) {
		error("In count_ntk_mark_chani: can not init mgl_table.");
		return -1;
	}
	e=iptc_first_rule(NTK_MARK_CHAIN,&mgl_table);
	while (e) {
		nchain++;
		e=iptc_next_rule(e,&mgl_table);
	}
	res=iptc_commit(&mgl_table);
	if (!res) {
		error("In count_ntk_mark_chain: can not close mgl_table.");
		return -1;
	}
	return nchain;
}

int delete_rule_if_exists(unsigned char *rule,const char *chain,int size)
{
	int res;
	res=maybe_rule_present(rule,chain,size);
	if (res==-1)
		return -1;
	if (res==0) {
		debug(DBG_NORMAL,"There is no rule to delete in chain %s.",chain);
		return 0;
	}
	res=mgl_table_init();
	if (res) {
		debug(DBG_NORMAL,"In delete_rule_if_exists: can not init mangle table.");
		return -1;
	}
	res=iptc_delete_num_entry(chain,0,&mgl_table);
	if (!res) {
		error("In delete_rule_if_exists: can not delete rule in chain %s.",chain);
		iptc_commit(&mgl_table);
		return -1;
	}
	res=iptc_commit(&mgl_table);
	if (!res) {
		error("In delete_rule_if_exists: can not commit for chain %s.",chain);
		return -1;
	}
	debug(DBG_NORMAL,"Deleted rule from chain %s.",chain);
	return 0;
}

int delete_ntk_forward_chain()
{
	int res;
	res=mgl_table_init();
	if (res==-1) {
		error("In delete_ntk_forward_chain: can not init mgl_table.");
		return -1;
	}
	res=iptc_flush_entries(NTK_MARK_CHAIN,&mgl_table);
        if (!res) {
        	error("In mark_init: -> %s", iptc_strerror(errno));
		return -1;
	}
	res=iptc_delete_chain(NTK_MARK_CHAIN,&mgl_table);
	if (!res) {
		error("In delete_ntk_forward_chain: can not delete ntk_mark_chain.");
		iptc_commit(&mgl_table);
		return -1;
	}
	res=iptc_commit(&mgl_table);
	if (!res) {
		error("In delete_ntk_forward_chain:: can not commit.");
		return -1;
	}
	debug(DBG_NORMAL,"Mangle chain ntk_mark_chain flushed and deleted.");
	return 0;
}

void mark_close()
{
	char rule[RESTORE_OUTPUT_RULE_SZ];
	char nrule[NTK_FORWARD_RULE_SZ];
	int res;
	int errs=0;
	
	restore_output_rule_init(rule);
	res=delete_rule_if_exists(rule,CHAIN_OUTPUT,RESTORE_OUTPUT_RULE_SZ);
	if (res==-1) errs++;
		
	ntk_forward_rule_init(nrule);
	res=delete_rule_if_exists(nrule,CHAIN_POSTROUTING,NTK_FORWARD_RULE_SZ);
	if (res==-1) errs++;

	res=delete_ntk_forward_chain();
	if (res==-1) errs++;
	loginfo("Mark_close exits with %d errors.", errs);
}
