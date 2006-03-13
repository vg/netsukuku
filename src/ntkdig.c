	         /**************************************
	        *     AUTHOR: Federico Tomassini        *
	       *     Copyright (C) Federico Tomassini    *
	      *     Contact federicotom@aliceposta.it     *
	     ***********************************************
	     *******          BEGIN 3/2006          ********
*************************************************************************
*                                              				* 
*  This program is free software; you can redistribute it and/or modify	*
*  it under the terms of the GNU General Public License as published by	*
*  the Free Software Foundation; either version 2 of the License, or	*
*  (at your option) any later version.					*
*									*
*  This program is distributed in the hope that it will be useful,	*
*  but WITHOUT ANY WARRANTY; without even the implied warranty of	*
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the	*
*  GNU General Public License for more details.				*
*									*
************************************************************************/
#include <getopt.h>  
#include <stdio.h>  

#include "ntkdig.h"
#include "andns.h"
#include "andns_mem.h"
#include "andns_pkt.h"

void print_usage() 
{
	printf("Usage:\n" 
		"\tntkdig [OPTIONS] host\n\n"
		" -v --version		print version, then exit.\n"
		" -n --nameserver=ns	use nameserver `ns' instead of localhost.\n"
		" -t --query-type=qt	query type (default A).\n"
/*		" -p --proto-type=pt	protocol to be used (default andns).\n"*/
		" -r --realm=realm	inet or netsukuku (default) realm to scan.\n"
		" -s --silent		ntkdig will be not loquacious.\n"
		" -h --help		display this help, then exit.\n\n");
}
void print_version()
{
	printf("ntkdig version %s (Netsukuku tools)\n\n",VERSION);
	printf("Copyright (C) 2006.\n"
		"This is free software.  You may redistribute copies of it under the terms of\n"
		"the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n"
		"There is NO WARRANTY, to the extent permitted by law.\n\n"
		"Written by Federico Tomassini.\n");

}
void init_opts()
{
	int res;
	memset(&globopts,0,NTKDIG_OPTS_SZ);
	res=ns_init(LOCALHOST,1);
	if (res) {
		printf("Internal error initializing options.\n");
		exit(1);
	}
	globopts.ns_len=1;
	globopts.ns_lhost=1;
	globopts.qt=QTYPE_A;
	globopts.pt=PROTO_ANDNS;
	globopts.realm=REALM_NTK;
}
	
int opt_set_ns(char *s,int limit)
{
	int res;
	res=ns_init(s,(limit)?1:MAX_NS);
	globopts.ns_lhost=0;
	return res;
}
uint8_t opt_set_qtype(char *s)
{
	uint8_t res;
	res=QTFROMPREF(s);
	globopts.qt=res;
	return res;
}
int opt_set_ptype(char *s)
{
	if (!strncasecmp(PROTO_ANDNS_STR,s,strlen(s)))
		globopts.realm=PROTO_ANDNS;
	else if (!strncasecmp(PROTO_DNS_STR,s,strlen(s)))
		globopts.realm=PROTO_DNS;
	else return -1;
	return 0;
}
int opt_set_realm(char *s)
{
	if (!strncasecmp(REALM_NTK_STR,s,strlen(s)))
		globopts.realm=REALM_NTK;
	else if (!strncasecmp(REALM_INT_STR,s,strlen(s)))
		globopts.realm=REALM_INT;
	else return -1;
	return 0;
}
int ns_init(const char *hostname,int nslimit)
{
	int res;
	struct addrinfo	*ailist,*aip,filter;

	memset(&filter,0,sizeof(struct addrinfo));
	filter.ai_family=AF_INET;
	filter.ai_socktype=SOCK_DGRAM;

	res=getaddrinfo(hostname,NTKDIG_PORT_STR,&filter,&ailist);
	if (res) {
		printf("Invalid address: %s\n",gai_strerror(errno));
		return -1;
	}
	res=min(MAX_NS,nslimit);
	for (aip=ailist;aip && globopts.ns_len<res;aip=aip->ai_next) {
		if (aip->ai_family!=AF_INET)
			continue;
		memcpy(globopts.ns+globopts.ns_len,aip->ai_addr,
				sizeof(struct sockaddr_in));
		globopts.ns_len++;
	}
	if (!globopts.ns_len) {
		printf("No nameserver found for %s.",hostname);
		return -1;
	}
	printf("Found %d nameservers.\n",globopts.ns_len);
	freeaddrinfo(ailist);
	return 0;
}

int ask_query(char *q,int qlen,char *an,int *anlen,struct sockaddr_in *saddr)
{
	int len,skt;
	skt=socket(PF_INET,SOCK_DGRAM,0);
	if (skt==-1) {
		printf("Internal error opening socket.\n");
		exit(1);
	}
			
	if ((connect(skt,(struct sockaddr*)saddr,sizeof(struct sockaddr_in)))) {
                printf("In ask_query: error connecting socket -> %s.",strerror(errno));
                return -1;
        }
	len=send(skt,q,qlen,0);
	if (len==-1) {
		printf("In ask_query: error sending pkt -> %s.\n",strerror(errno));
		return -1;
	}
	len=recv(skt,an,ANDNS_MAX_SZ,0);
	if (len==-1) {
		printf("In ask_query: error receiving pkt -> %s.\n",strerror(errno));
		return -1;
	}
	*an=len;
	return 0;
}

int do_command(const char *s)
{
	int res,msglen,answlen;
	andns_pkt *ap;
	char msg[ANDNS_MAX_SZ],answ[ANDNS_MAX_SZ];
	int i;

	printf("Quering for %s\n",s);
	ap=create_andns_pkt();
	ap->id=rand()>>16;
	ap->qtype=globopts.qt;
	ap->nk=(globopts.realm==REALM_NTK)?NK_NTK:NK_INET;
	ap->qstlength=strlen(s);
	memcpy(ap->qstdata,s,ap->qstlength);
	msglen=apktpack(ap,msg);
	if (msglen==-1) {
		printf("Internal error building packet.");
		exit(1);
	}
	res=0;
	for (i=0;i<globopts.ns_len;i++) {
		res=ask_query(msg,msglen,answ,&answlen,globopts.ns+i);
		if (res==-1)
			continue;
		else 
			break;
	}
	if (res==-1) {
		printf("Sending packet failed.\n");
		exit(1);
	}
	else 
		printf("Uau!\n");
	msglen=apkt(answ,answlen,&ap);
	if (msglen==-1) {
		printf("Answer interpretation error.\n");
		exit(1);
	}
	if (msglen!=answlen) 
		printf("Answer interpretation: answer stream was %d. Readed %d bytes.\n",answlen,msglen);
	return 0;
			
}
	/* 
	 * Query is ntk-related and directed to localhost:53 
	 * No need to send packet.
	 * Use andna.
	 */
	/*if (globopts.realm==REALM_NTK && globopts.ns_lhost) {
		switch (globopts.qt) {
			case QTYPE_A:
				res=andna_resolve_hname(s, &ipres);
				if (res==-1) 
					printf("Ntk host %s does not exist.\n");
				else
					printf("%s\n",inet_to_str(ipres));
				exit(0);
				break;
			case QTYPE_PTR:
				char **hnames;
				res=str_to_inet(s,&ipres);
				if (res==-1) {
					printf("Invalid address %s\n",s);
					exit(1);
				}
				res=andna_reverse_resolve(ipres,&hnames);
				if (res==-1) {
					printf("Address %s has no hostname\n",s);
					exit(1);
				}
				for(i=0;i<res;i++) {
					printf("%s\n",hnames+i);
					xfree(hnames+i);
				}
				xfree(hnames);
				exit(0);
				break;
			case QTYPE_MX:
				printf("Not implemented.\n");
				exit(0);
				break;
			case QTYPE_MXPTR:
				printf("Not implemented.\n");
				exit(0);
				break;
			default:
				printf("Where do you wish to go today? :P\n");
				exit(1);
				break;
		}
	}*/
	/* query is ntk-related, but directed to remote host. Use andns proto.*/
	/*if (globopts.realm==REALM_NTK) {
	}
	else {
		printf("Not implemented.\n");
		exit(1);
	}
	return 0;
}*/



int main(int argc,char **argv) 
{
	int c,res;
	extern int optind, opterr, optopt;
	extern char *optarg;

	init_opts();
	struct option longopts[]= {
		{"version",0,0,'v'},
		{"nameserver",1,0,'n'},
		{"query-type",1,0,'t'},
		{"realm",1,0,'r'},
		{"silent",0,0,'s'},
		{"help",0,0,'h'},
		{0,0,0,0}
	};

	while (1) {
		int option_index=0;
		c=getopt_long(argc, argv, "vn:t:p:r:sh", longopts, &option_index);
		if (c==-1) 
			break;
		switch (c) {
			case 'v':
				print_version();
				exit(1);
			case 's':
				globopts.silent=1;
				break;
			case 'h':
				print_usage();
				exit(1);
			case 'n':
				res=opt_set_ns(optarg,0);
				if (res) {
					printf("Nameserver specified is not friendly.\n");
					exit(1);
					break;
				}
				break;
			case 't':
				res=opt_set_qtype(optarg);
				if (res==-1) {
					printf("Query type %s is not valid.\n\n"
						"Valid query types are:\n"
						" a\thost -> ip\n"
						" mx\thost -> mx\n"
						" ptr\tip -> host\n"
						" ptrmx\tip ->mx\n"
						"(or univoque abbreviation)\n\n",optarg);
					exit(1);
					break;
				}
				break;
		/*	case 'p':
				res=opt_set_ptype(optarg);
				if (res) {
					printf("Proto type %s is not valid.\n\n"
						"Valid protocols are:\n"
						" andns\tandns protocol\n"
						" dns\tdns protocol\n"
						"(or univoque abbreviation)\n\n",optarg);
					exit(1);
					break;
				}
				break;*/
			case 'r':
				res=opt_set_realm(optarg);
				if (res) {
					printf("%s is not a valid realm.\n\n"
						"Valid realms are:\n"
						" ntk\tnetsukuku realm\n"
						" inet\tinternet realm\n"
						"(or univoque abbreviation)\n\n",optarg);
					exit(1);
					break;
				}
				break;

			/*case '?':
				printf("111%s: option `-%c' is invalid: ignored\n",argv[0], optopt);
				break;*/
			default:
				print_usage();
				exit(1);
				break;
		}
	}
	if (optind==argc) {
		print_usage();
		exit(1);
	}
	res=do_command(argv[optind]);
	return 0;
}
			
				

	

	

