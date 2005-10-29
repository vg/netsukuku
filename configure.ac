AC_PREREQ(2.59)
AC_INIT(netsukuku, 0.0.4b)
AM_INIT_AUTOMAKE(netsukuku, 0.0.4b)
AM_CONFIG_HEADER(src/config.h src/netsukuku.conf) 

AC_PROG_CC
AC_PROG_CPP
AC_PROG_CXX

AC_CHECK_HEADERS([pthread.h openssl/crypto.h gmp.h])
AC_ARG_WITH(conf_dir, AS_HELP_STRING([--with-conf-dir], \
[Netsukuku conf dir (default /usr/share/netsukuku)],\
[conf_dir = "$withval"], [conf_dir="/usr/share/netsukuku"]))

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

