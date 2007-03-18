#ifndef ANDNS_SHARED_H
#define ANDNS_SHARED_H

#include "andns_lib.h"
#include "andns_net.h"

#define ANDNS_SHARED_VERSION "0.0.1"

typedef struct andns_query
{
    char    question[ANDNS_MAX_NTK_HNAME_LEN];
    int     hashed;
    int     recursion;
    int     type;
    int     realm;
    int     proto;
    int     service;
    char    *errors;
    int     status;
    char    **answers;
    char    *andns_server;
    int     port;
} andns_query;

void andns_set_error(const char *err, andns_query *q);
void hstrcpy(char *d, char *s);
int andns_set_ntk_hname(andns_query *q, andns_pkt *p);
int andns_set_question(andns_query *q, andns_pkt *p);
int andns_dialog(andns_query *q, andns_pkt *ap);
andns_pkt *ntk_query(andns_query *query);

#endif
