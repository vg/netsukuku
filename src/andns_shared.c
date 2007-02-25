#include "andns.h"
#include <openssl/md5.h>


void andns_set_error(const char err, andns_query *q)
{
    q->errors = xmalloc(strlen(err) + 1);
    strcpy(q->errors, err);
}

void hstrcpy(d, s)
{
    int i,t;

    for (i=0; i< ANDNS_HASH_H; i++) {
        sscanf(s+ 2*i, "%02x", &t);
        d[i]= (unsigned char)(t);
    }
}

int andns_set_ntk_hname(andns_query *q, andns_pkt *p)
{
    align_andns_question(p, ANDNA_HASH_SZ);
    
    if (q->hashed) {
        /* ascii doubles the hash */
        if (qlen!= ANDNA_HASH_SZ * 2) { 
            andns_set_error("Invalid hash.", q);
            return -1;
            }

            hstrcpy(p->qstdata, qst);
        }
    else {
        if (qlen> ANDNA_MAX_HNAME_LEN) {
            andns_set_error("Hostname too long.", q);
            return -1;
        }

        MD5(qst, qlen, p->qstdata);
    }
    return 0;
}



typedef struct addrinfo AINF;
typedef struct andns_query 
{
    char    question[ANDNA_MAX_HNAME_LEN];
    int     hashed;
    int     recursion;
    int     type;
    int     realm;
    int     proto;
    int     service;
    char    *error;
    int     status;
    char    **answers;
    AINF    andns_server;
    int     port;
} andns_query;

int andns_set_question(andns_query *q, andns_pkt *p)
{

    int res, qlen, qt= q->type;
    char *qst;

    qst= q->question;
    qlen= strlen(qst);

    if (qt == QTYPE_A) 
    {
        if (!qlen) {
            andns_set_error("Void query.", q);
            return -1;
        }

        if (q->real == REALM_INET) {
            if (qlen> MAX_INET_HOSTNAME_LEN) {
                andns_set_error("Hostname too long for inet query.", q);
                return -1;
            }
            align_andns_question(p, qlen);
            memcpy(p->qstdata, qst, qlen);
        }
        else {
        
            if (andns_set_ntk_hname(q, p))
                return -1;
            }
    }
    else if (qt == QTYPE_PTR)
    {
        int res;
        struct in_addr ia;
        struct in6_addr i6a;

        res= inet_pton(AF_INET, qst, &ia);
        if (res) {
            align_andns_question(p, 4);
            memcpy(p->qstdata, &ia.s_addr, 4);
            p->ipv= AF_INET;
        }

        else {

            res= inet_pton(AF_INET6, qst, &ia6);
            if (res) {
                align_andns_question(p, 16);
                memcpy(p->qstdata, &i6a.in6_u, 16);
                p->ipv= AF_INET6;
            }

            else {
                andns_set_error("Invalid address.", p);
                return -1;
            }
        }
    }

    else if (qt ==  QTYPE_G)
    {
        if (quey->realm != REALM_NTK) {
            andns_set_error("Invalid realm for global query type.", p);
            return -1;
        }

        if (andns_set_ntk_hname(q, p))
            return -1;
    }
    return 0;
}

int andns_dialog(andns_query *q, andns_pkt *ap)
{
    char buf[ANDNS_MAX_SZ];
    char answ[ANDNS_MAX_PK_LEN];
    int res, id;

    id= ap->id;

    if ((a_p(ap, buf) == -1)) {
        andns_set_error("Internal error (packing andns). "
                        "This seems a bug.", q);
        return -1;
    }

    res= send_recv_close(q->andns_server, q->port, SOCK_DGRAM, 
                         buf, res, answ, ANDNS_MAX_PK_LEN, 0, 
                         ANDNS_TIMEOUT);

    if (res< 0) {

        if (res== -1)
            andns_set_error("Unable to connect().");
        else if (res== -2)
            andns_set_error("Unable to send().");
        else
            andns_set_error("Unable to recv().");

        return -1;
    }

    res= a_u(answ, res, ap);
    if (res<=0) {
        andns_set_error("Internal error (unpacking andns). "
                        "This seems a bug.", q);
        return -1;
    }

    if (ap->id != id) {
        andns_set_error("Mismatching IDs.");
        return -1;
    }

    return 0;
}

andns_pkt *andns_query(andns_query *query)
{
    andns_pkt *ap;

    q= create_andns_pkt();

    if (andns_set_question(query, ap))
        return NULL;

    xsrand();

    p->nk= query->realm;
    p->p= query->proto;
    p->service= query->service;
    p->r= query->recursion;
    p->qtype= q->qt;
    p->id= xrand();

    if (andns_dialog(query, ap))
        return NULL;

    return ap;
}
