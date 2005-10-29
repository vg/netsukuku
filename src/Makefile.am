SRCDIRS = man
AM_CFLAGS= -Wall -g -I. 
AM_CXXFLAGS = -Wall -g -I. 
bin_PROGRAMS = netsukuku_d
netsukuku_d_SOURCES= accept.c andna.c andna_cache.c bmap.c conf.c \
	crypto.c daemon.c dns_pkt.cpp dns_wrapper.c endianness.c \
	gmap.c hook.c if.c inet.c ipv6-gmp.c krnl_route.c krnl_rule.c \
	libnetlink.c ll_map.c llist.c log.c map.c misc.c netsukuku.c \
	pkts.c qspn.c request.c radar.c route.c tracer.c \
	xmalloc.c
netsukuku_d_LDADD = -lpthread -lgmp -lcrypto
