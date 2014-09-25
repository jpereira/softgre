# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Qui 25 Set 2014 10:47:53 BRT
####

AC_INIT([SoftGRE],
        [0.1],
        [https://github.com/jpereira/softgre/issues],
        [softgre],
        [https://github.com/jpereira/softgre/])

AM_INIT_AUTOMAKE 
AC_CONFIG_SRCDIR([src/softgred.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

# enable debug/devel mode
AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug], [enable debugging messages (default enabled)])],
              AC_DEFINE([DEBUG], [0], [debug mode off]),
              AC_DEFINE([DEBUG], [1], [debug mode on]))

AC_ARG_ENABLE(devel,
              [AS_HELP_STRING([--enable-devel], [enable development messages (default enabled)])],
              AC_DEFINE([DEVEL], [0], [devel mode off]),
              AC_DEFINE([DEVEL], [1], [devel mode on]))

# check for programs
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_INLINE
AC_C_BIGENDIAN

# checks for header files
AC_CHECK_HEADERS([stdio.h stddef.h stdbool.h stdlib.h string.h ctype.h stdarg.h pthread.h])

# network headers
AC_CHECK_HEADERS([netinet/in.h sys/time.h fcntl.h unistd.h sys/socket.h ifaddrs.h])

# checks for typedefs, structures, and compiler characteristics
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T

# check externals
AC_PROG_SED

# enable maintainer mode
AM_MAINTAINER_MODE([enable])
PKG_PROG_PKG_CONFIG([0.16])

## check for libraries
# pthread
AC_CHECK_LIB([pthread], [pthread_mutex_init], [], [
            echo "pthread library is missing. pthread is required for this program"
                        exit -1])
# Check for getifaddrs
AM_CONDITIONAL([SOFTGRE_HAVE_SELF_IFADDRS], [test "x$ac_cv_header_ifaddrs_h" != "xyes"])

# RFC2553 introduce sockaddr_storage as ifa_addr member in ifaddrs structure
# Not all distros follow this.
AS_IF([test "x$ac_cv_header_ifaddrs_h" = "xyes"],
  [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <net/if.h>
    #include <ifaddrs.h>
    ]], [[
    struct ifaddrs *myaddrs;
    getifaddrs (&myaddrs);
    if (myaddrs->ifa_addr->ss_family == AF_INET) {
    }
    ]])],
    [have_sockaddr_storage=yes],
    [have_sockaddr_storage=no])],
  [AS_IF([test "x$have_sockaddr_storage" = "xyes"],
    [AC_DEFINE([RFC2553], [], [Define if RFC2553 is followed])])])

# TODO: Alterar para verificar com pkg-config
# check for libpcap 
AC_CHECK_LIB([pcap],[pcap_create])
AC_CHECK_HEADERS([pcap/pcap.h])
#LIBPCAP_LIBS='$(pcap-config --libs)'
LIBPCAP_LIBS='-lpcap'
AC_SUBST([LIBPCAP_LIBS])

# check for libnl-1
AC_CHECK_HEADERS([netlink/attr.h])
LIBNL_LIBS='-lnl'
AC_SUBST([LIBNL_LIBS])

# check for libnfnetlink
AC_CHECK_HEADERS([libnfnetlink/libnfnetlink.h])
LIBNL_LIBS='-lnfnetlink -lnetlink'
AC_SUBST([LIBNFNETLINK_LIBS])

# libpthread
LIBPTHREAD_LIBS='-lpthread'
AC_SUBST([LIBPTHREAD_LIBS])

# resolv
AC_CHECK_LIB([resolv], [herror])
AC_SEARCH_LIBS([strerror],[cposix])
AC_CHECK_FUNCS([gettimeofday])

# Makefiles
AC_CONFIG_FILES([
Makefile
bin/Makefile
])

AC_OUTPUT
