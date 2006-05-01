#include "snsd.h"
#include <string.h>

/*
 * Writes 
 * NULL on error.
 */
int snsd_main_ip(char *hname_hash,snsd_node *dst)
{
	snsd_service *ss,sst;
	snsd_prio *sp;
	snsd_node *sn;
	int records;

	ss=snsd_resolve_hash(hname_hash,0,0,*records);
	if (!ss) 
		err_ret(ERR_SNDMRF,0);
	if (!(sp=ss->prio)) {
		FREE;
		err_ret(ERR_SNDMRF,0);
	}
	list_for(sp) 
		list_for(sp->node) 
			if (sp->node->flags & SNSD_NODE_MAIN_IP) {
				memcpy(dst,sp->node,sizeof(snsd_node));
				FREE;
				return 0;
			}
	FREE;
	err_ret(ERR_SNDMRF,-1);
}

/*
 * Converts a snsd_node struct to andns data.
 * data means a packed answer.
 * buf has to be ANDNS_MAX_ANSW_IP_LEN long.
 *
 * returns -1 on error, answer len otherwise.
 *
 */

int snsd_node_to_aansw(char *buf,snsd_node *sn,u_char prio)
{
	int res;
	if (sn->flags & SNSD_NODE_IP ||
	    sn->flags & SNSD_NODE_MAIN_IP) {
		memcpy(buf+2,sn->record,10); /* TODO */
	} else {
		snsd_node snt;
		res=snsd_main_ip(sn->record,&snt);
		if (res) {
			error(err_str);
			return -1;
		}
		res=snsd_node_to_data(buf,&snt,prio); /* this is now a safe call */
		return res;
	}
	*buf++=sn->weigth;
	*buf=prio;
	return 10+2; /* TODO */
}

/*
 * Converts a snsd_prio list to andns data.
 * data means a set of contiguous answers ready 
 * to be sent.
 *
 * Returns bytes write and stores in count the number of
 * answers packed.
 * buf has to be long enough, ie, you have to count node
 * in prio list and take ANDNS_MAX_ANSW_IP_LEN * n space.
 *
 */
int snsd_prio_to_aansw(char *buf,snsd_prio *sp,int *count)
{
	int res=0,rest;
	snsd_node *sn;
	u_char prio=sp->prio;
	
	*count=0;
	sn=sp->node;
	list_for(sn) {
		rest=snsd_node_to_data(buf,sn,prio);
		if (rest==-1)
			continue;
		res=+rest;
		buf+=rest;
		(*count)++;
	}
	return res;
}

/*
 * Differently from snsd_node_to_aansw, the function
 * writes only the ip data on buf.
 */
int snsd_node_to_data(char *buf,snsd_node *sn,int iplen)
{
	int res;
	int family;
        if (sn->flags & SNSD_NODE_IP ||
            sn->flags & SNSD_NODE_MAIN_IP) {
                memcpy(buf,sn->record,iplen); 
		family==(iplen==4)?AF_INET:AF_INET6;
		inet_htonl(buf,family);
        } else {
                snsd_node snt;
                res=snsd_main_ip(sn->record,&snt);
                if (res) {
                        error(err_str);
                        return -1;
                }
                res=snsd_node_to_data(buf,&snt); /* this is now a safe call */
        }
        return 0;
}
/*
 * Given a dns_packet, this function add an answer to it
 * and returns 0;
 * Otherwise returns -1 and nothing has to be done.
 */
int snsd_node_to_dp_answ(dns_pkt *dp,snsd_node *sn,int iplen)
{
	char temp[16];
	dns_pkt_a *dpa;

	if (snsd_node_to_data(temp,sn,iplen))
		return -1;
	dpa=DP_ADD_ANSWER(dp);
	dns_a_default_fill(dp,dpa);
	dpa->rdlength=_ip_len_;
	memcpy(dpa->rdata,temp,iplen);
	return 0;
}
/*
 * Converts a snsd_prio struct, adding a set of answers to
 * the dns_packet dp.
 * Returns the number of answers added to dp.
 */
int snsd_prio_to_dp_answs(dns_pkt *dp,snsd_prio *sp,int iplen)
{
	int res=0;
	snsd_node *sn;
	
	sn=sp->node;
	listfor(sn) 
		if (!snsd_node_to_dp_answ(dp,sn,iplen))
			res++;
	return res;
}
		
		
