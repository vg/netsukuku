#ifndef ANDNS_SHARED_H
#define ANDNS_SHARED_H

#include "andns.h"

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
    char    *errors;
    int     status;
    char    **answers;
    AINF    andns_server;
    int     port;
} andns_query;

void andns_set_error(const char *err, andns_query *q);
void hstrcpy(char *d, char *s);
int andns_set_ntk_hname(andns_query *q, andns_pkt *p);
int andns_set_question(andns_query *q, andns_pkt *p);
int andns_dialog(andns_query *q, andns_pkt *ap);
andns_pkt *ntk_query(andns_query *query);

#endif
