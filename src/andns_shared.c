#include "andns_shared.h"
#include "crypto.h"


void andns_set_error(const char *err, andns_query *q)
{
    q->errors = (char*)xmalloc(strlen(err) + 1);
    strcpy(q->errors, err);
}

void hstrcpy(char *d, char *s)
{
    int i;
    char t;

    for (i=0; i< ANDNS_HASH_HNAME_LEN; i++) {
        sscanf((const char*)(s+ 2*i), "%02x", &t);
        *(d+i)= t;
    }
}

int andns_set_ntk_hname(andns_query *q, andns_pkt *p)
{
    int qlen= strlen(q->question);

    align_andns_question(p, ANDNS_HASH_HNAME_LEN);
    
    if (q->hashed) {
        /* ascii doubles the hash */
        if (qlen!= ANDNS_HASH_HNAME_LEN * 2) { 
            andns_set_error("Invalid hash.", q);
            return -1;
        }

        hstrcpy(p->qstdata, q->question);
    }
    else {
        if (qlen> ANDNS_MAX_NTK_HNAME_LEN) {
            andns_set_error("Hostname too long.", q);
            return -1;
        }

        MD5(q->question, qlen, p->qstdata);
    }
    return 0;
}

int andns_set_question(andns_query *q, andns_pkt *p)
{

    int res, qlen, qt= q->type;
    char *qst;

    qst= q->question;
    qlen= strlen(qst);

    if (qt == AT_A) 
    {
        if (!qlen) {
            andns_set_error("Void query.", q);
            return -1;
        }

        if (q->realm == ANDNS_INET_REALM) {
            if (qlen> ANDNS_MAX_INET_HNAME_LEN) {
                andns_set_error("Hostname too long for inet query.", q);
                return -1;
            }
            align_andns_question(p, qlen);
            memcpy(p->qstdata, qst, qlen);
        }
        else if (andns_set_ntk_hname(q, p)) return -1;
    }
    else if (qt == AT_PTR)
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
            res= inet_pton(AF_INET6, qst, &i6a);
            if (res) {
                align_andns_question(p, 16);
                memcpy(p->qstdata, &i6a.in6_u, 16);
                p->ipv= AF_INET6;
            }
            else {
                andns_set_error("Invalid address.", q);
                return -1;
            }
        }
    }

    else if (qt ==  AT_G)
    {
        if (q->realm != ANDNS_NTK_REALM) {
            andns_set_error("Invalid realm for global query type.", q);
            return -1;
        }

        if (andns_set_ntk_hname(q, p))
            return -1;
    }
    return 0;
}

int andns_dialog(andns_query *q, andns_pkt *ap)
{
    char buf[ANDNS_PKT_QUERY_SZ];
    char answ[ANDNS_PKT_TOT_SZ];
    int res, id;

    id= ap->id;

    if ((a_p(ap, buf) == -1)) {
        andns_set_error("Internal error (packing andns). "
                        "This seems a bug.", q);
        return -1;
    }

    res= send_recv_close(q->andns_server, q->port, SOCK_DGRAM, 
                         buf, res, answ, ANDNS_PKT_TOT_SZ, 0, 
                         ANDNS_TIMEOUT);

    if (res< 0) {

        if (res== -1)
            andns_set_error("Unable to connect().", q);
        else if (res== -2)
            andns_set_error("Unable to send().", q);
        else
            andns_set_error("Unable to recv().", q);

        return -1;
    }

    res= a_u(answ, res, &ap);
    if (res<=0) {
        andns_set_error("Internal error (unpacking andns). "
                        "This seems a bug.", q);
        destroy_andns_pkt(ap);
        return -1;
    }

    if (ap->id != id) {
        andns_set_error("Mismatching IDs.", q);
        destroy_andns_pkt(ap);
        return -1;
    }

    return 0;
}

andns_pkt *ntk_query(andns_query *query)
{
    andns_pkt *ap;

    ap= create_andns_pkt();

    if (andns_set_question(query, ap))
        return NULL;

    xsrand();

    ap->nk      = query->realm;
    ap->p       = query->proto;
    ap->service = query->service;
    ap->r       = query->recursion;
    ap->qtype   = query->type;
    ap->id      = xrand();

    if (andns_dialog(query, ap))
        return NULL;

    return ap;
}
