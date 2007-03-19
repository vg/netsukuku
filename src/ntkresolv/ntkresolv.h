#ifndef NTK_RESOLV_H
#define NTK_RESOLV_H

#include <andns.h>

#define NTK_RESOLV_VERSION      "0.3.4"

#define REALM_NTK_STR   "ntk"
#define REALM_INT_STR   "inet"

#define QTYPE_A         AT_A
#define QTYPE_PTR       AT_PTR
#define QTYPE_G         AT_G
#define QTYPE_MX        3
#define QTYPE_A_STR     "snsd"
#define QTYPE_PTR_STR   "ptr"
#define QTYPE_G_STR     "global"
#define QTYPE_MX_STR    "mx"

#define ANDNS_PROTO_TCP_STR  "tcp"
#define ANDNS_PROTO_UDP_STR  "udp"


/* NK BIT */
#define NK_DNS  0
#define NK_NTK  ANDNS_NTK_REALM
#define NK_INET ANDNS_INET_REALM

#define HELP_STR   "help"



char *QTYPE_STR_LIST[]={QTYPE_A_STR,QTYPE_PTR_STR,QTYPE_G_STR,QTYPE_MX_STR};
int QT_LEN=4;
#define QTFROMPREF(s)                                       \
({                                                          \
    int __n,__res=-1;                                       \
    for (__n=0;__n<QT_LEN;__n++)                            \
        if (!strncasecmp(s,QTYPE_STR_LIST[__n],strlen(s)))  \
            {__res=__n;break;}                              \
    __res; })           

#define REALMFROMPREF(s)                                    \
({                                                          \
    uint8_t __res=0;                                        \
    if (!strncasecmp(REALM_NTK_STR,s,strlen(s)))            \
        __res=ANDNS_NTK_REALM;                              \
    else if (!strncasecmp(REALM_INT_STR,s,strlen(s)))       \
        __res=ANDNS_INET_REALM;                             \
        __res; })   

#define PROTOFROMPREF(s)                                    \
({                                                          \
    uint8_t __res=-1;                                       \
    if (!strncasecmp(ANDNS_PROTO_UDP_STR,s,strlen(s)))      \
        __res=ANDNS_PROTO_UDP;                              \
    else if (!strncasecmp(ANDNS_PROTO_TCP_STR,s,strlen(s))) \
        __res=ANDNS_PROTO_TCP;                              \
        __res; })   

        
#define QR_STR(ap)  ((ap)->qr==0)?"QUERY":"ANSWER"
#define QTYPE_STR(ap)                                       \
({                                                          \
    char *__c;                                              \
    switch((ap)->qtype) {                                   \
        case QTYPE_A:                                       \
            __c="Host2Ip";                                  \
            break;                                          \
        case QTYPE_PTR:                                     \
            __c="Ip2Host";                                  \
            break;                                          \
        case QTYPE_G:                                       \
            __c=" Global";                                  \
            break;                                          \
        default:                                            \
            __c="Unknow";                                   \
            break;                                          \
            }                                               \
        __c;})

#define NK_STR(ap)                                          \
({                                                          \
    char *__d;                                              \
    switch((ap)->nk) {                                      \
        case NK_DNS:                                        \
            __d="DNS";                                      \
            break;                                          \
        case NK_NTK:                                        \
            __d="Ntk";                                      \
            break;                                          \
        case NK_INET:                                       \
            __d="Inet";                                     \
            break;                                          \
        default:                                            \
            __d="UNKNOW";                                   \
            break;                                          \
            }                                               \
        __d;})

#define RCODE_STR(ap)                                       \
({                                                          \
    char *__e;                                              \
    switch((ap)->rcode) {                                   \
        case ANDNS_RCODE_NOERR:                             \
            __e="NoError";                                  \
            break;                                          \
        case ANDNS_RCODE_EINTRPRT:                          \
            __e="InError";                                  \
            break;                                          \
        case ANDNS_RCODE_ESRVFAIL:                          \
            __e="SrvFail";                                  \
            break;                                          \
        case ANDNS_RCODE_ENSDMN:                            \
            __e="NoXHost";                                  \
            break;                                          \
        case ANDNS_RCODE_ENIMPL:                            \
            __e="NotImpl";                                  \
            break;                                          \
        case ANDNS_RCODE_ERFSD:                             \
            __e="Refused";                                  \
            break;                                          \
        default:                                            \
            __e="UNKNOW";                                   \
            break;                                          \
    }                                                       \
    __e;})

#define IPV_STR(ap)                                         \
({                                                          \
    char *__f;                                              \
    switch((ap)->ipv) {                                     \
        case ANDNS_IPV4:                                    \
            __f="IPv4";                                     \
            break;                                          \
        case ANDNS_IPV6:                                    \
            __f="IPv6";                                     \
            break;                                          \
        default:                                            \
            __f="UNKNOW";                                   \
            break;                                          \
    }                                                       \
    __f;})

#define MAX_INT_STR 10

#define SERVICE_STR(ap)                                     \
({                                                          \
    char *__g;                                              \
    char __t[MAX_INT_STR];                                  \
    switch((ap)->qtype) {                                   \
        case QTYPE_G:                                       \
            __g="*";                                        \
            break;                                          \
        case QTYPE_PTR:                                     \
            __g="None";                                     \
            break;                                          \
        case QTYPE_A:                                       \
            snprintf(__t,MAX_INT_STR,"%d",                  \
                ap->service);                               \
            __g=__t;                                        \
            break;                                          \
        default:                                            \
            __g="UNKNOW";                                   \
            break;                                          \
    }                                                       \
    __g;})

#define PROTO_STR(ap)                                       \
({                                                          \
    char *__h;                                              \
    switch((ap)->qtype) {                                   \
        case QTYPE_G:                                       \
            __h="*";                                        \
            break;                                          \
        case QTYPE_PTR:                                     \
            __h="None";                                     \
            break;                                          \
        case QTYPE_A:                                       \
            if (!ap->service)                               \
                __h="None";                                 \
            else                                            \
                __h=ap->p==ANDNS_PROTO_TCP?                 \
                ANDNS_PROTO_TCP_STR:                        \
                ANDNS_PROTO_UDP_STR;                        \
            break;                                          \
        default:                                            \
            __h="UNKNOW";                                   \
            break;                                          \
    }__h;})
    

#define NTK_RESOLV_HASH_STR(s,d)                            \
({                                                          \
    int __i;                                                \
    for (__i=0;__i<ANDNS_HASH_HNAME_LEN;__i++)              \
        sprintf(d+2*__i,"%02x",((unsigned char*)(s))[__i]); \
    d[2*ANDNS_HASH_HNAME_LEN]=0;})

#define NTK_RESOLV_STR_HASH(s,d)                            \
({                                                          \
    int __i,__t;                                            \
    for (__i=0;__i<ANDNS_HASH_HNAME_LEN;__i++) {            \
        sscanf(s+2*__i,"%02x",&__t);                        \
        d[__i]=(unsigned char)(__t);}})

#define NTK_RESOLV_IP_SYMBOL    "~"
#define NTK_RESOLV_HNAME_SYMBOL "-"
#define NTK_RESOLV_SYMBOL(apd)  (apd)->m&ANDNS_APD_IP?      \
                    NTK_RESOLV_IP_SYMBOL:                   \
                    NTK_RESOLV_HNAME_SYMBOL

/* FUNCTIONS */

void version(void);
void usage(void);
void qt_usage(char *arg);
void realm_usage(char *arg);
void proto_usage(char *arg);
void service_and_proto_usage(char *arg);
double diff_time(struct timeval a,struct timeval b);
void print_headers(andns_pkt *ap);
void print_question(andns_query *q);
void ip_bin_to_str(andns_pkt *ap, void *data,char *dst);
void answer_data_to_str(andns_pkt *ap, andns_pkt_data *apd, char *dst);
void print_answers(andns_query *q, andns_pkt *ap);
void print_parsable_answers(andns_query *q, andns_pkt *ap);
void print_results(andns_query *q, andns_pkt *ap);
void do_command(andns_query *q, const char *qst);
void opts_init(andns_query *q);
void opts_set_ns(andns_query *q, const char *h);
void opts_set_qt(andns_query *q, char *arg);
void opts_set_realm(andns_query *q, char *arg);
void opts_set_service_and_proto(andns_query *q, char *arg);
void opts_set_proto(andns_query *q, char *arg);
void compute_hash(const char *arg);
int main(int argc, char **argv);


#endif /* NTK_RESOLV_H */
