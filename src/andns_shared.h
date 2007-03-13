#ifndef ANDNS_SHARED_H
#define ANDNS_SHARED_H

#include "andns.h"
#include "andna_cache.h"

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

#endif
