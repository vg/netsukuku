#ifndef NTK_DIG_H
#define NTK_DIG_H

#include <errno.h>
#include <netdb.h>
#include <strings.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "andns_lib.h"

#define VERSION			"0.1"

#define NTKDIG_PORT		53
#define NTKDIG_PORT_STR		"53"
#define MAX_NS			3
#define LOCALHOST		"localhost"

#define MAX_HOSTNAME_LEN	512
#define NTKDIG_MAX_OBJ_LEN	512

//#define ANDNS_MAX_SZ    1024

#define min(x,y)		(x)<(y)?(x):(y)

#define REALM_NTK		0+1
#define REALM_INT		1+1
#define REALM_NTK_STR		"ntk"
#define REALM_INT_STR		"inet"

#define QTYPE_A			0
#define QTYPE_PTR		1
#define QTYPE_A_STR		"snsd"
#define QTYPE_PTR_STR		"ptr"

#define SNSD_PROTO_TCP		0
#define SNSD_PROTO_UDP		1
#define SNSD_PROTO_TCP_STR	"tcp"
#define SNSD_PROTO_UDP_STR	"udp"

/* NK BIT */
#define NK_DNS			0
#define NK_NTK                  1
#define NK_INET                 2

#define TIME_SCALE		1000000.0

char *QTYPE_STR_LIST[]={QTYPE_A_STR,QTYPE_PTR_STR};
int QT_LEN=2;

#define QTFROMPREF(s)							\
({									\
 	int __n,__res=-1;						\
	for (__n=0;__n<QT_LEN;__n++) 					\
		if (!strncasecmp(s,QTYPE_STR_LIST[__n],strlen(s))) 	\
 			{__res=__n;break;}				\
	__res; })			
#define REALMFROMPREF(s)						\
({									\
	uint8_t __res=0;						\
	if (!strncasecmp(REALM_NTK_STR,s,strlen(s)))			\
		__res=REALM_NTK;					\
	else if (!strncasecmp(REALM_INT_STR,s,strlen(s)))		\
 		__res=REALM_INT; 					\
		__res; })	
#define PROTOFROMPREF(s)						\
({									\
 	uint8_t __res=2;						\
	if (!strncasecmp(SNSD_PROTO_UDP_STR,s,strlen(s)))		\
		__res=SNSD_PROTO_UDP;					\
	else if (!strncasecmp(SNSD_PROTO_TCP_STR,s,strlen(s)))		\
 		__res=SNSD_PROTO_TCP; 					\
		__res; })	

		
			
typedef struct ntkdig_opts {
	char		nsserver[MAX_HOSTNAME_LEN];
	int16_t		port;
	int8_t		silent;
	char		obj[NTKDIG_MAX_OBJ_LEN];
	andns_pkt	*q;
} ntkdig_opts;

#define NTKDIG_OPTS_SZ	sizeof(ntkdig_opts)

#define QR_STR(ap)	((ap)->qr==0)?"QUERY":"ANSWER"
#define QTYPE_STR(ap)						\
({								\
 	char *__c;						\
 	switch((ap)->qtype) {					\
 		case AT_A:					\
			__c="Host2Ip";				\
			break;					\
 		case AT_PTR:					\
			__c="Ip2Host";				\
			break;					\
/* 		case AT_MX:					\
			__c="Host2MX";				\
			break;					\
 		case AT_MXPTR:					\
			__c="Ip2MX";				\
			break;					\*/\
		default:					\
			__c="Unknow";				\
 			break;					\
			}					\
		__c;})						
#define NK_STR(ap)						\
({								\
	char *__d;						\
	switch((ap)->nk) {					\
		case NK_DNS:					\
			__d="DNS";				\
			break;					\
		case NK_NTK:					\
			__d="Ntk";				\
			break;					\
		case NK_INET:					\
			__d="Inet";				\
			break;					\
		default:					\
			__d="UNKNOW";				\
 			break;					\
			}					\
 		__d;})						

#define RCODE_STR(ap)						\
({								\
 	char *__e;						\
	switch((ap)->rcode) {					\
		case ANDNS_RCODE_NOERR:				\
			__e="NOERR";				\
			break;					\
		case ANDNS_RCODE_EINTRPRT:			\
			__e="INTRPRT";				\
			break;					\
		case ANDNS_RCODE_ESRVFAIL:			\
			__e="SRFFAIL";				\
			break;					\
		case ANDNS_RCODE_ENSDMN:			\
			__e="NOXHOST";				\
			break;					\
		case ANDNS_RCODE_ENIMPL:			\
			__e="NOTIMPL";				\
			break;					\
		case ANDNS_RCODE_ERFSD:				\
			__e="REFUSED";				\
			break;					\
		default:					\
			__e="UNKNOW";				\
			break;					\
	}							\
	__e;})
#define IPV_STR(ap)						\
({								\
 	char *__f;						\
 	switch((ap)->ipv) {					\
		case ANDNS_IPV4:				\
 			__f="IPv4";				\
 			break;					\
		case ANDNS_IPV6:				\
 			__f="IPv6";				\
 			break;					\
		default:					\
			__f="UNKNOW";				\
			break;					\
	}							\
	__f;})
#define QST_STR(ap)						\
({								\
 	char *__g;
 	

#define GET_OPT_REALM	(globopts.realm==REALM_NTK)?"NTK":"INET"

/* CODE UTILS */
#define say             printf
#define bye             if (!AMISILENT) say("\tBye!\n");

#define GOP             globopts
#define AMISILENT       GOP.silent
#define GQT             GOP.q

#define COMPUTE_TIME    diff_time(time_start,time_stop)
#define time_report     if (!AMISILENT){gettimeofday(&time_stop,NULL);      \
                        say("\nQuery time: %f seconds.\n"                   \
                                        ,COMPUTE_TIME);}

#define G_ALIGN(len)    GQT->qstlength=len;GQT->qstdata=(char*)  	    \
                                xmalloc(len);          		            \
                                if (!GQT->qstdata){say("Fatal malloc!\n");  \
                                        exit(1);}
#define G_SETQST_A(s)   G_ALIGN(strlen(s));strcpy(GQT->qstdata,s);          \
                                GQT->qstlength=strlen(s);

/* FUNCTIONS */
void version(void);
void usage(void);
void qt_usage(char *arg);
void realm_usage(char *arg);
void proto_usage(char *arg);
double diff_time(struct timeval a,struct timeval b);
void opts_init(void);
void opts_set_silent();
void opts_set_port(char *arg);
void opts_set_ns(char *arg);
void opts_set_qt(char *arg);
void opts_set_realm(char *arg);
void opts_set_service(char *arg);
void opts_set_proto(char *arg) ;
void do_command();
int main(int argc, char **argv);

#endif /* NTK_DIG_H */
