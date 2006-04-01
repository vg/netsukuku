SUBDIRS = man scripts conf

AM_CFLAGS= -g -I. @debug_flags@ -I@gmp@
AM_CXXFLAGS = -g -I. @debug_flags@ -I@gmp@

bin_PROGRAMS = netsukuku_d
netsukuku_d_SOURCES= accept.c map.c misc.c pkts.c qspn.c request.c radar.c   \
	gmap.c hook.c rehook.c if.c inet.c ipv6-gmp.c krnl_route.c \
	andna.c andna_cache.c andns.c andnslib.c dnslib.c bmap.c \
	conf.c crypto.c daemon.c dns_wrapper.c endianness.c   \
	krnl_rule.c iptunnel.c libnetlink.c ll_map.c llist.c log.c \
	route.c tracer.c igs.c libping.c xmalloc.c netsukuku.c \
	mark.c libiptc/libip4tc.c
netsukuku_d_LDADD = -lpthread -lgmp -lcrypto -lresolv
