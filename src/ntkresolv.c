#include "includes.h"

#include "ntkresolv.h"
#include "andns_net.h"
#include "snsd_cache.h"
#include "crypto.h"
#include "misc.h"
#include "xmalloc.h"
#include "log.h"

static ntkresolv_opts globopts;
static struct timeval time_start,time_stop;

void version(void)
{
        say("ntk-resolv version %s (Netsukuku tools)\n\n"
            "Copyright (C) 2006.\n"
            "This is free software.  You may redistribute copies of it under the terms of\n"
            "the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
            "There is NO WARRANTY, to the extent permitted by law.\n\n",VERSION);
	ntkresolv_safe_exit(1);
}


void usage(void)
{
        say("Usage:\n"
                "\tntk-resolv [OPTIONS] host\n\n"
                " -v --version          print version, then exit.\n"
                " -n --nameserver=ns    use nameserver `ns' instead of localhost.\n"
                " -P --port=port        nameserver port, default 53.\n"
                " -t --query-type=qt    query type (`-t help' shows more info).\n"
                " -r --realm=realm      realm to scan (`-r help' shows more info).\n"
                " -s --service=service  SNSD service (`-s help' shows more info).\n"
                " -p --protocolo=proto  SNSD protocol (`-p help' shows more info).\n"
                " -S --silent           ntk-resolv will be not loquacious.\n"
                " -h --help             display this help, then exit.\n\n");
	ntkresolv_safe_exit(1);
}
void qt_usage(char *arg)
{
	if (arg)
		say("Bad Query Type %s\n\n",arg);
	else
		say("ntk-resolv Query Type Help.\n\n");
	say(
	    "Valid query types are:\n"
            " * snsd\thost:port -> ip\n"
            "   ptr\tip -> host\n"
            "   global\thostname -> all services ip\n"
            "   mx\thostname MX -> ip\n\n"
            "(you can also use univoque abbreviation)\n"
	    "Note: mx query is equivalent to --query-type="
	    "snsd AND --service=25\n\n");
	ntkresolv_safe_exit(1);
}
void realm_usage(char *arg)
{
	if (arg)
		say("Bad Realm %s\n\n",arg);
	else
		say("ntk-resolv Realm Help.\n\n");
	say(
	    "Valid realms are:\n"
            " * ntk\tnetsukuku realm\n"
            "   inet\tinternet realm\n\n"
            "(you can also use univoque abbreviation)\n\n");
	ntkresolv_safe_exit(1);
}
void proto_usage(char *arg)
{
	if (arg)
		say("Bad Protocol %s\n\n",arg);
	else
		say("ntk-resolv Protocol Help.\n\n");
	say(
	    "Valid protocols are:\n"
            " * tcp\n"
            "   udp\n"
            "(you can also use univoque abbreviation)\n"
	    "Note: you can also specify the protocol with option `-s'.\n" 
	    "To know more, type:\n"
	    "\tntk-resolv -s help\n\n");
	ntkresolv_safe_exit(1);
}
void service_and_proto_usage(char *arg)
{
	if (arg)
		say("Bad service/proto %s\n\n"
			"Use `ntk-resolv -s help` for more info on"
			" service and proto.\n"	,arg);
	else say(
		"ntk-resolv Service and Proto Help.\n\n"
		"The form to specify a service and a protocol are:\n"
		"  ntk-resolv -s service/proto\n"
		"  ntk-resolv -s service -p proto\n\n"
		"Valid protocols are:\n"
	        " * tcp\n"
            	"   udp\n\n"
		"Valid services are expressed in /etc/services.\n"
		"You can use numeric form too.\n\n"
		"As example, the next commands are equivalent and\n"
		"will return the IP of the hostname that offers\n"
		"webpages for the hostname \"some_hostname\":\n\n"
		"  ntk-resolv -s http -p tcp some_hostname\n"
		"  ntk-resolv -s http/tcp    some_hostname\n"
		"  ntk-resolv -s 80/tcp      some_hostname\n"
		"  ntk-resolv -s 80          some_hostname\n\n");
	ntkresolv_safe_exit(1);
}
		

double diff_time(struct timeval a,struct timeval b)
{
        double res;
        res=(double)(b.tv_sec-a.tv_sec);
	if (res<0.9 || b.tv_usec>=a.tv_usec)
		res+=(b.tv_usec-a.tv_usec)/TIME_SCALE;
	else {
		res-=1.0;
		res+=(TIME_SCALE+b.tv_usec-a.tv_usec)/TIME_SCALE;
	}
        return res;
}


void opts_init(void)
{
	memset(&GOP,0,NTKRESOLV_OPTS_SZ);
	strcpy(GOP.nsserver,LOCALHOST);
	GOP.port=NTKRESOLV_PORT;
	GQT=create_andns_pkt();
	GQT->nk=REALM_NTK;
	GQT->p=SNSD_DEFAULT_PROTO;
	GQT->service=SNSD_DEFAULT_SERVICE;
	xsrand();
}

void opts_set_silent(void)
{
	GOP.silent=1;
}

void opts_set_port(char *arg)
{
	int res;
	uint16_t port;

	res=atoi(arg);
	port=(uint16_t)res;

	if (port!=res) {
		say("Bad port %s.",arg);
		ntkresolv_safe_exit(1);
	}
	GOP.port=port;
}

void opts_set_ns(char *arg)
{
	int slen;

	slen=strlen(arg);
	if (slen>=MAX_HOSTNAME_LEN) {
		say("Server hostname too long.");
		ntkresolv_safe_exit(1);
	}
	strcpy(GOP.nsserver,arg);
	GOP.nsserver[slen]=0;
}

void opts_set_qt(char *arg)
{
	int res;

	if (!strcmp(arg,HELP_STR))
		qt_usage(NULL);
	res=QTFROMPREF(arg);
	if (res==-1) 
		qt_usage(arg);
	if (res==QTYPE_MX) {
		GQT->qtype=QTYPE_A;
		GQT->service=25;
		GQT->p=SNSD_PROTO_TCP;
	} else
		GQT->qtype=res;
}

void opts_set_realm(char *arg)
{
	uint8_t res;

	if (!strcmp(arg,HELP_STR))
		realm_usage(NULL);
	res=REALMFROMPREF(arg);
	if (!res) 
		realm_usage(arg);
	GQT->nk=res;
}

void opts_set_service_and_proto(char *arg)
{
	int ret;

	if (!strcmp(arg,HELP_STR))
		service_and_proto_usage(NULL);
	ret=str_to_snsd_service(arg, (int *)&GQT->service, &GQT->p);
/*	if(ret == -1)
		say("Bad service %s.",arg);
	else if(ret == -2)
		proto_usage(arg);*/
	if(ret < 0)
		service_and_proto_usage(arg);
}
void opts_set_proto(char *arg) 
{
	int ret;

	if (!strcmp(arg,HELP_STR))
		proto_usage(NULL);
	ret=PROTOFROMPREF(arg);
	if (ret<0)
		proto_usage(arg);
	GQT->p=ret;
}

/*void hname_hash(char *dst,char *src)
{
	u_char hashm5[ANDNS_HASH_H];
	u_int hval=0;
	
	hash_md5(src, strlen(src), hashm5);

	 * TODO:
	 * why do you use this second hash?
	 * andna_resolve_hash works only with a full md5 hash
	bp = (u_char *)hashm5; 
        be = bp + 16;         
    	while (bp < be) {
        	hval += (hval<<1) + (hval<<4) + (hval<<7) + (hval<<8) + (hval<<24);
        	hval ^= (u_long)*bp++;
    	}
	hval=htonl(hval);
	memcpy(dst,hashm5,ANDNS_HASH_H);
}*/

void opts_set_question(char *arg)
{
	struct in_addr ia;
	struct in6_addr i6a;
	int res;
	
	res=strlen(arg);
	if (res>NTKRESOLV_MAX_OBJ_LEN) {
		say("Object requested is too long: %s",arg);
		ntkresolv_safe_exit(1);
	}
	strcpy(GOP.obj,arg);

	switch(GQT->qtype) {
		case QTYPE_A:
			if (GQT->nk==REALM_NTK) {
				G_ALIGN(ANDNS_HASH_H);
				hash_md5(arg,res,GQT->qstdata);
			} else {
				res=strlen(arg);
				if (res>255) {
					say("Hostname %s is too long for DNS standard.",arg);
					ntkresolv_safe_exit(1);
				}
				G_ALIGN(res+1);
				strcpy(GQT->qstdata,arg);
			}
			return;
		case QTYPE_PTR:
			res=inet_pton(AF_INET,arg,&ia);
			if (res) {
				G_ALIGN(ANDNS_HASH_H);
				memcpy(GQT->qstdata,&ia.s_addr,4);
				return;
			}
			res=inet_pton(AF_INET6,arg,&i6a);
			if (!res) {
				say("Bad address `%s'\n",arg);
				ntkresolv_safe_exit(1);
			}
			G_ALIGN(16);
			memcpy(GQT->qstdata,&i6a.in6_u,16);
			GQT->ipv=ANDNS_IPV6;
			return;
		case QTYPE_G:
			if (GQT->nk!=REALM_NTK) {
				say("Global query type is valid only for the Ntk realm.");
				ntkresolv_safe_exit(1);
			}
			G_ALIGN(ANDNS_HASH_H);
			hash_md5(arg,res,GQT->qstdata);
			return;	
		default:
			say("Unknow Query Type.\n");
			return;
	}
}
void opts_finish(char *arg)
{
	int r;

	r=rand();
	GQT->id=r>>16;
	opts_set_question(arg);
}

void print_headers()
{
	if (AMISILENT)
		return;
	andns_pkt *ap=GQT;
	say("\n - Headers Section:\n"
		"\tid ~ %6d\tqr  ~ %4d\tqtype ~ %7s\n"
		"\tan ~ %6d\tipv ~ %s\trealm ~ %7s\n"
		"\trCode ~ %s\n",
		ap->id,ap->qr,QTYPE_STR(ap),
		ap->ancount,IPV_STR(ap),NK_STR(ap),
		RCODE_STR(ap));
}
void print_question()
{
	if (AMISILENT)
		return;
	
	say("\n - Question Section:\n"
		"\tObj ~ %s\n",GOP.obj);
}

void ip_bin_to_str(void *data,char *dst)
{
	int family;
	struct in_addr ia;
	struct in6_addr i6a;
	const void *via;
	const char *crow;

	family=GQT->ipv==ANDNS_IPV4?
		AF_INET:AF_INET6;
	switch(family) {
		case AF_INET:
			memcpy(&(ia.s_addr),data,4);
			via=(void*)(&ia);
			break;
		case AF_INET6:
			memcpy(&(i6a.in6_u),data,16);
			via=(void*)(&i6a);
			break;
		default:
			strcpy(dst,"Unprintable Object");
			return;
	}
	crow=inet_ntop(family,via,dst,NTKRESOLV_MAX_OBJ_LEN);
	if (!crow) 
		strcpy(dst,"Unprintable Object");
}

void answer_data_to_str(andns_pkt_data *apd,char *dst)
{
	switch(GQT->qtype) {
		case AT_PTR || AT_G:
			strcpy(dst,apd->rdata);
			break;
		case AT_A:
			ip_bin_to_str(apd->rdata,dst);
			break;
		default:
			strcpy(dst,"Unprintable Object");
			break;
	}
}
void print_answers()
{
	int i=0;
	int ancount=GQT->ancount;
	andns_pkt_data *apd;

	if (!ancount)
		return;

	say("\n - Answers Section:\n");

	apd=GQT->pkt_answ;
	while (apd) {
		i++;
		if (i>ancount) 
			say("Answer not declared in Headers Packet.\n");
		answer_data_to_str(apd,GOP.obj);
		say("\t ~ %s",GOP.obj);
		if (apd->m)
			say(" * SNSD Primary IP");
		say("\n");
		if (GQT->qtype==AT_A) 
			say("\t\tPrio ~ %d  Weigth ~ %d\n\n",
				apd->prio,apd->wg);
		apd=apd->next;
	}
}


void do_command(void)
{
	char buf[ANDNS_MAX_SZ];
	char answer[ANDNS_MAX_PK_LEN];
	int res;

	memset(buf,0,ANDNS_MAX_SZ);
	res=a_p(GQT,buf);
	if (res==-1) {
		say("Error building question.\n");
		ntkresolv_exit(1);
	}
	res=hn_send_recv_close(GOP.nsserver,GOP.port,
			SOCK_DGRAM,buf,res,answer,ANDNS_MAX_PK_LEN,0);
	if (res==-1) {
		say("Communication failed with %s.\n",GOP.nsserver);
		ntkresolv_exit(1);
	}
	res=a_u(answer,res,&GQT);
	if (res<=0) {
		say("Error interpreting server answer.\n");
		ntkresolv_exit(1);
	}
	print_headers();
	print_question();
	print_answers();
	destroy_andns_pkt(GQT);
}
void ntkresolv_exit(int i)
{
	exit(i);
}
void ntkresolv_safe_exit(int i)
{
	destroy_andns_pkt(GQT);
	ntkresolv_exit(i);
}
int main(int argc, char **argv)
{
        int c;
        extern int optind, opterr, optopt;
        extern char *optarg;

	log_init("",0,1);
	gettimeofday(&time_start,NULL);

	opts_init();
	struct option longopts[]= {
                {"version",0,0,'v'},
                {"nameserver",1,0,'n'},
                {"port",1,0,'P'},
                {"query-type",1,0,'t'},
                {"realm",1,0,'r'},
                {"service",1,0,'s'},
                {"proto",1,0,'p'},
                {"silent",0,0,'S'},
                {"help",0,0,'h'},
                {0,0,0,0}
        };

	while(1) {
		int oindex=0;
		c=getopt_long(argc, argv, 
			"vn:P:t:r:s:p:Sh", longopts, &oindex);
		if (c==-1)
			break;
		switch(c) {
			case 'v':
				version();
			case 'n':
				opts_set_ns(optarg);
				break;
			case 'P':
				opts_set_port(optarg);
				break;
			case 't':
				opts_set_qt(optarg);
				break;
			case 'r':
				opts_set_realm(optarg);
				break;
			case 's':
				opts_set_service_and_proto(optarg);
				break;	
			case 'p':
				opts_set_proto(optarg);
				break;	
			case 'h':
				usage();
			case 'S':
				opts_set_silent();
				break;
			default:
				usage();
		}
	}
	if (optind==argc)
		usage();
	opts_finish(argv[optind]);
	do_command();
	time_report;
	bye;
	return 0;
}
