#ifndef MARK_H
#define MARK_H

#include "libiptc/libiptc.h"
#include <linux/netfilter_ipv4/ip_conntrack.h>
#include <linux/netfilter_ipv4/ip_conntrack_tuple.h>
#include "libiptc/ipt_conntrack.h"
#include "libiptc/ipt_connmark.h"
#include "libiptc/ipt_CONNMARK.h"


#define MANGLE_TABLE		"mangle"
#define FILTER_TABLE		"filter"
#define NTK_MARK_CHAIN		"ntk_mark_chain"
#define CHAIN_OUTPUT		"OUTPUT"
#define CHAIN_POSTROUTING	"POSTROUTING"
#define CHAIN_FORWARD		"FORWARD"
#define TUNNEL_IFACE		"tunl"

#define MOD_CONNTRACK		"conntrack"
#define MOD_CONNMARK		"CONNMARK"

#define NTK_NET_STR		"10.0.0.0"
#define NTK_NET_MASK_STR	"255.0.0.0"

iptc_handle_t mgl_table;
iptc_handle_t ntk_mrk_chain;

#define IPT_ENTRY_SZ		sizeof(struct ipt_entry)
#define IPT_ENTRY_MATCH_SZ	sizeof(struct ipt_entry_match)
#define IPT_ENTRY_TARGET_SZ	sizeof(struct ipt_entry_target)
#define IPT_CT_INFO_SZ		sizeof(struct ipt_conntrack_info)
#define IPT_CM_TARGET_INFO_SZ	sizeof(struct ipt_connmark_target_info)

#define MATCH_SZ		IPT_ENTRY_MATCH_SZ+IPT_CT_INFO_SZ
#define TARGET_SZ		IPT_ENTRY_TARGET_SZ+IPT_CM_TARGET_INFO_SZ

#define RESTORE_OUTPUT_RULE_SZ	IPT_ENTRY_SZ+MATCH_SZ+TARGET_SZ	

#define OFFSET_MATCH		IPT_ENTRY_SZ
#define OFFSET_MATCH_INFO	OFFSET_MATCH+IPT_ENTRY_MATCH_SZ
#define OFFSET_TARGET		OFFSET_MATCH_INFO+IPT_CT_INFO_SZ
#define OFFSET_TARGET_INFO	OFFSET_TARGET+IPT_ENTRY_TARGET_SZ
		
#define MARK_RULE_SZ		IPT_ENTRY_SZ+TARGET_SZ
#define MAX_MARK_RULES		100

#define NTK_FORWARD_RULE_SZ	OFFSET_TARGET_INFO+4

#define FILTER_RULE_SZ		IPT_ENTRY_SZ+TARGET_SZ
#define INET_MARK		25


/* Functions */

int mgl_table_init();
void restore_output_rule_init(unsigned char *rule);
int output_rule_commit();
void ntk_forward_rule_init(unsigned char *rule);
int ntk_forward_rule_commit();
int maybe_rule_present(unsigned char *rule,const char *chain,int rule_sz);
void mark_rule_init(unsigned char *rule);
int fill_mark_rule(unsigned char *rule,char *outiface,int outiface_num);
int create_mark_rules(int n);
int mark_init(void);
int count_ntk_mark_chain();
int delete_rule_if_exists(unsigned char *rule,const char *chain,int size);
int delete_ntk_forward_chain();
void mark_close();

#endif /* MARK_H */
