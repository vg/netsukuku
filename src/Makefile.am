SUBDIRS = man scripts conf

AM_CFLAGS= -g -I. @debug_flags@ -I@gmp@
AM_CXXFLAGS = -g -I. @debug_flags@ -I@gmp@

bin_PROGRAMS = ntkd ntk-dig
ntkd_SOURCES = accept.c map.c misc.c pkts.c qspn.c request.c radar.c \
	gmap.c hook.c rehook.c if.c inet.c ipv6-gmp.c krnl_route.c   \
	andna.c snsd_cache.c andna_cache.c 			     \
	andns.c andns_lib.c dnslib.c andns_net.c andns_snsd.c	     \
	bmap.c  conf.c crypto.c daemon.c dns_wrapper.c endianness.c  \
	krnl_rule.c iptunnel.c libnetlink.c ll_map.c llist.c log.c   \
	route.c tracer.c igs.c libping.c xmalloc.c netsukuku.c       \
	mark.c libiptc/libip4tc.c
ntkd_LDADD = -lpthread -lgmp -lcrypto -lresolv -lz

ntk_dig_SOURCES = andns_lib.c ntkdig.c andns_net.c crypto.c 	     \
		  snsd_cache.c inet.c ll_map.c libnetlink.c
ntk_dig_LDADD = -lz -lcrypto
