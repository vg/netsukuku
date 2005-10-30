AC_PREREQ(2.59)
AC_INIT(netsukuku, 0.0.4b)
AM_INIT_AUTOMAKE(netsukuku, 0.0.4b)
AC_DEFINE(VERSION, "0.0.4b")
AM_CONFIG_HEADER(src/config.h src/netsukuku.conf) 

dnl -----------------------------------
dnl Get hostname and other information.
dnl -----------------------------------
AC_CANONICAL_HOST

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

dnl Some systems (Solaris 2.x) require libnsl (Network Services Library)
AC_MSG_CHECKING(OS)
case "$host" in
  *-sunos5.6* | *-solaris2.6*)
      opsys=sol2-6
      AC_DEFINE(SUNOS_5,,SunOS 5)
      AC_CHECK_LIB(xnet, main)
      AC_MSG_RESULT(solaris)
  ;;
  *-sunos5* | *-solaris2*)
      AC_DEFINE(SUNOS_5,,SunOS 5)
      AC_CHECK_LIB(socket, main)
      AC_CHECK_LIB(nsl, main)
      AC_MSG_RESULT(solaris)
  ;;
  *-linux-*)
      opsys=gnu-linux
      AC_DEFINE(GNU_LINUX,,GNU Linux)
      AC_MSG_RESULT(linux)
  ;;
  *-nec-sysv4*)
      AC_CHECK_LIB(nsl, gethostbyname)
      AC_CHECK_LIB(socket, socket)
      AC_MSG_RESULT(net-sysv4)
  ;;
  *-freebsd*)
      AC_DEFINE(FREEBSD,,FreeBSD)
      AC_MSG_RESULT(freebsd)
  ;;
  *-openbsd*)
      opsys=openbsd
      AC_DEFINE(OPEN_BSD,,OpenBSD)
      AC_MSG_RESULT(openbsd)
  ;;
  *-bsdi*)
      opsys=bsdi
      OTHER_METHOD="mtu_kvm.o"
      AC_CHECK_LIB(kvm, main)
      AC_MSG_RESULT(bsdi)
  ;;
  *-irix6.5)
      opsys=irix
      AC_DEFINE(IRIX_65,,IRIX 6.5)
      AC_MSG_RESULT(irix)
  ;;
esac

dnl ------------------------------------
dnl Determine routing get and set method
dnl ------------------------------------
AC_MSG_CHECKING(Netsukuku kernel interface method)
if test x"$opsys" = x"gnu-linux"; then
    AC_MSG_RESULT(netlink)
    RT_METHOD=rt_netlink.o
    AC_DEFINE(HAVE_NETLINK,,netlink)
    netlink=yes
else
  if test "$opsys" = "sol2-6";then
    AC_MSG_RESULT(solaris)
    KERNEL_METHOD="kernel_socket.o"
    RT_METHOD="rt_socket.o"
  elif test "$opsys" = "irix" ; then
    AC_MSG_RESULT(irix)
    KERNEL_METHOD="kernel_socket.o"
    RT_METHOD="rt_socket.o"
  else
    AC_TRY_RUN([#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>

main ()
{
  int ac_sock;

  ac_sock = socket (AF_ROUTE, SOCK_RAW, 0);
  if (ac_sock < 0 && errno == EINVAL)
    exit (1);
  exit (0);
}],
  [KERNEL_METHOD=kernel_socket.o
   RT_METHOD=rt_socket.o
   AC_MSG_RESULT(socket)],
  [RT_METHOD=rt_ioctl.o
   AC_MSG_RESULT(ioctl)],
  [KERNEL_METHOD=kernel_socket.o
   RT_METHOD=rt_socket.o
   AC_MSG_RESULT(socket)])
  fi
fi
AC_SUBST(RT_METHOD)
AC_SUBST(KERNEL_METHOD)
AC_SUBST(OTHER_METHOD)


dnl -----------------------------
dnl check interface lookup method
dnl -----------------------------
AC_MSG_CHECKING(interface looking up method)
if test "$netlink" = yes; then
  AC_MSG_RESULT(netlink)
  IF_METHOD=if_netlink.o
else
  if test "$opsys" = "sol2-6";then
    AC_MSG_RESULT(solaris)
    IF_METHOD=if_ioctl.o
  elif test "$opsys" = "irix" ; then
    AC_MSG_RESULT(irix)
     IF_METHOD=if_ioctl.o
  elif test "$opsys" = "openbsd";then
    AC_MSG_RESULT(openbsd)
    IF_METHOD=if_ioctl.o
  elif grep NET_RT_IFLIST /usr/include/sys/socket.h >/dev/null 2>&1; then
    AC_MSG_RESULT(sysctl)
    IF_METHOD=if_sysctl.o
    AC_DEFINE(HAVE_NET_RT_IFLIST,,NET_RT_IFLIST)
  else
    AC_MSG_RESULT(ioctl)
    IF_METHOD=if_ioctl.o
  fi
fi
AC_SUBST(IF_METHOD)


AC_ARG_WITH(conf_dir, AS_HELP_STRING([--with-conf-dir], \
[Netsukuku conf dir (default /usr/share/netsukuku)],\
[conf_dir = "$withval"], [conf_dir="/usr/share/netsukuku"]))

AC_ARG_ENABLE(debug, AS_HELP_STRING([--enable-debug], [Enable Netsukuku debug]),\
[debug_flags="-Wall -DDEBUG"], [debug_flags=""])
AC_SUBST(debug_flags)

AC_CONFIG_FILES([Makefile src/Makefile src/man/Makefile])
AC_OUTPUT

