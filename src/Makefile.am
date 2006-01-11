SUBDIRS = man
AM_CFLAGS= -g -I. @debug_flags@ -I@gmp@
AM_CXXFLAGS = -g -I. @debug_flags@ -I@gmp@
bin_PROGRAMS = netsukuku_d
netsukuku_d_SOURCES= accept.c andna.c andna_cache.c bmap.c conf.c  \
	crypto.c daemon.c dns_pkt.cpp dns_wrapper.c endianness.c   \
	gmap.c hook.c rehook.c if.c inet.c ipv6-gmp.c krnl_route.c \
	krnl_rule.c iptunnel.c libnetlink.c ll_map.c llist.c log.c \
	map.c misc.c netsukuku.c pkts.c qspn.c request.c radar.c   \
	route.c tracer.c igs.c libping.c xmalloc.c
netsukuku_d_LDADD = -lpthread -lgmp -lcrypto
sysconf_DATA = netsukuku.conf ip_masquerade.sh
