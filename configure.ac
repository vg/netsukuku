AC_PREREQ(2.59)
AC_INIT(netsukuku, 0.0.4b)
AM_INIT_AUTOMAKE(netsukuku, 0.0.4b)
AC_DEFINE(VERSION, "0.0.4b")
AM_CONFIG_HEADER(src/config.h src/netsukuku.conf) 

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX

sysconfdir=/etc/netsukuku
prefix=/usr

AC_HEADER_STDC
AC_CHECK_HEADERS([unistd.h])


AC_ARG_WITH(gmp-include, AS_HELP_STRING([--with-gmp-include], \
[gmp include dir]), [gmp="$withval"])
AC_SUBST(gmp)

AC_ARG_WITH(boost-include, AS_HELP_STRING([--with-boost-include], \
[boost include dir]), [boost="$withval"])
AC_SUBST(boost)



AC_CHECK_HEADERS([pthread.h], ,\
	[echo "============================ Unable to find pthread.h"; \
	 echo "Read README first!!!!!" ])
AC_CHECK_HEADERS([openssl/crypto.h], ,\
	[echo "============================ Unable to find openssl/crypto.h";\
	 echo "Read README first!!!!!"])
AC_CHECK_HEADERS([gmp.h], ,\
	[echo "============================ Unable to find gmp.h"; \
	 echo "Read README first!!!!!"])

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable Netsukuku debug]),\
[debug_flags="-Wall -DDEBUG"], [debug_flags=""])
AC_SUBST(debug_flags)

AC_CONFIG_FILES([Makefile src/Makefile src/man/Makefile])
AC_OUTPUT

