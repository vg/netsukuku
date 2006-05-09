#include "includes.h"

#include "llist.c"
#include "andns_snsd.h"
#include "err_errno.h"
#include "andna.h"
#include "log.h"



	/* h2ip functions */

/*
 * Given a a hostname hash, makes a resolution 
 * call (service=0) and search the main ip entry,
 * storing it to snsd_node dst.
 *
 * Returns:
 * 	0
 * 	-1
 */
int snsd_main_ip(u_int *hname_hash,snsd_node *dst)
{
	snsd_service *ss;
	snsd_prio *sp;
	snsd_node *sn;
	int records;

	ss=andna_resolve_hash(hname_hash,0,0,&records);
	if (!ss) 
		err_ret(ERR_SNDMRF,-1);
	if (!(sp=ss->prio)) {
		goto destroy_return;
	}
	list_for(sp) {
		sn=sp->node;
		list_for(sn) 
			if (sn->flags & SNSD_NODE_MAIN_IP) {
				memcpy(dst,sn,sizeof(snsd_node));
				snsd_service_llist_del(&ss);
				return 0;
			}
	}
	goto destroy_return;
destroy_return:
	snsd_service_llist_del(&ss);
	err_ret(ERR_SNDMRF,-1);
}

/*
 * Convert a snsd_node to a binary ip.
 * If snsd_node does not contain a ip, but a hostname hash,
 * calls another resolution with service=0.
 *
 * Returns:
 * 	0
 * 	-1
 */
int snsd_node_to_data(char *buf,snsd_node *sn,int iplen)
{
	int res;
	int family;
        if (sn->flags & SNSD_NODE_IP ||
            sn->flags & SNSD_NODE_MAIN_IP) {
                memcpy(buf,sn->record,iplen); 
		family=(iplen==4)?AF_INET:AF_INET6;
		inet_htonl((u_int*)buf,family);
        } else {
                snsd_node snt;
                res=snsd_main_ip(sn->record,&snt);
                if (res) {
                        error(err_str);
                        err_ret(ERR_SNDRCS,-1);
                }
                res=snsd_node_to_data(buf,&snt,iplen); /* this is now a safe call */
		return res;
        }
        return 0;
}

/*
 * Converts a snsd_node struct to andns data.
 * data means a packed answer.
 * buf has to be ANDNS_MAX_ANSW_IP_LEN long.
 *
 * returns -1 on error, answer len otherwise.
 *
 */

size_t snsd_node_to_aansw(char *buf,snsd_node *sn,u_char prio,int iplen)
{
	int res;

	res=snsd_node_to_data(buf+2,sn,iplen);
	if (res==-1) {
		error(err_str);
		return -1;
	}
	if (sn->flags & SNSD_NODE_MAIN_IP)
		*buf|=0x80;
	*buf++=sn->weight;
	*buf=prio;
	return 0; /* TODO */
}
/*
 * Converts a snsd_prio list to andns data.
 * data means a set of contiguous answers ready 
 * to be sent.
 *
 * Returns the number of answers writed to buf.
 * The size is computable with iplen.
 *
 * buf has to be long enough, ie, you have to count node
 * in prio list and take ANDNS_MAX_ANSW_IP_LEN * n space.
 *
 */
int snsd_prio_to_aansws(char *buf,snsd_prio *sp,int iplen)
{
	int res;
	int count=0;
	snsd_node *sn;
	
	sn=sp->node;
	list_for(sn) {
		res=snsd_node_to_aansw(buf,sn,sp->prio,iplen);
		if (res==-1) 
			continue;
		count++;
		buf+=2+iplen; 
	}
	return count;
}

/*
 * Given a dns_packet, this function add an answer to it
 * and returns 0;
 * Otherwise returns -1.
 */
int snsd_node_to_dansw(dns_pkt *dp,snsd_node *sn,int iplen)
{
	char temp[16];
	dns_pkt_a *dpa;

	if (snsd_node_to_data(temp,sn,iplen))
		return -1;
	dpa=DP_ADD_ANSWER(dp);
	dns_a_default_fill(dp,dpa);
	dpa->rdlength=iplen;
	memcpy(dpa->rdata,temp,iplen);
	return 0;
}
/*
 * Converts a snsd_prio struct, adding a set of answers to
 * the dns_packet dp.
 * Returns the number of answers added to dp.
 */
int snsd_prio_to_dansws(dns_pkt *dp,snsd_prio *sp,int iplen)
{
	int res=0;
	snsd_node *sn;
	
	sn=sp->node;
	list_for(sn) 
		if (!snsd_node_to_dansw(dp,sn,iplen))
			res++;
	return res;
}
		
		
		
	/* ip2h functions */

/*
 * Converts a lcl_cache struct to a set of dns answers.
 * Returns the number of answers added.
 */
int lcl_cache_to_dansws(dns_pkt *dp,lcl_cache *lc)
{
	dns_pkt_a *dpa;
	int res=0;
	
	list_for(lc) {
		dpa=DP_ADD_ANSWER(dp);
		dns_a_default_fill(dp,dpa);
		strcpy(dpa->rdata,lc->hostname);
		res++;
	}
	lcl_cache_free(lc);
	return res;
}

/* 
 * Converts a lcl_cache to andns data. 
 * Returns the number of bytes writed.
 */
size_t lcl_cache_to_aansws(char *buf,lcl_cache *lc,int *count)
{
	uint16_t slen;
	size_t ret=0;
	
	list_for(lc) {
		slen=strlen(lc->hostname);
		slen=htons(slen);
		memcpy(buf,&slen,2);
		buf+=2;
		strcpy(buf,lc->hostname);
		ret+=2+slen;
	}
	return ret;
}
