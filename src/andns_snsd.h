#ifndef ANDNS_SNSD_H
#define ANDNS_SNSD_H

#include "dnslib.h"
#include "andns_lib.h"
#include "andna_cache.h"

int snsd_main_ip(u_int *hname_hash,snsd_node *dst);
size_t snsd_node_to_aansw(char *buf,snsd_node *sn,u_char prio,int iplen);
int snsd_node_to_data(char *buf,snsd_node *sn,int iplen);
int snsd_prio_to_aansws(char *buf,snsd_prio *sp,int iplen);
int snsd_node_to_dansw(dns_pkt *dp,snsd_node *sn,int iplen);
int snsd_prio_to_dansws(dns_pkt *dp,snsd_prio *sp,int iplen);
int lcl_cache_to_dansws(dns_pkt *dp,lcl_cache *lc);
size_t lcl_cache_to_aansws(char *buf,lcl_cache *lc,int *count);

#endif /* ANDNS_SNSD_H */
