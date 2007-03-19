#include <sys/time.h>
#include <stdlib.h>

#include "andns.h"
#include "andns_lib.h"
#include "andns_net.h"
#include "andns_shared.h"

andns_pkt *ntk_query(andns_query *query)
{
    andns_pkt *ap;
    struct timeval randgen;
    unsigned short x[3];

    ap= create_andns_pkt();

    if (andns_set_question(query, ap))
        return NULL;

    gettimeofday(&randgen, 0);
    x[0]= (ushort) (randgen.tv_usec);
    x[1]= (ushort) (randgen.tv_usec >> 16);
    x[2]= (ushort) getpid();

    ap->qtype   = query->type;
    ap->nk      = query->realm;
    ap->p       = query->proto;
    ap->service = query->service;
    ap->r       = query->recursion;
    ap->id      = (uint16_t) nrand48(x);

    if (andns_dialog(query, ap))
        return NULL;

    return ap;
}

void free_andns_pkt(andns_pkt *ap)
{
    destroy_andns_pkt(ap);
    return;
}
