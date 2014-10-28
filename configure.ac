dnl Author: Jorge Pereira <jpereiran@gmail.com>
dnl Created: Mon Sep 22 23:14:38 EDT 2014
dnl Last Change: Ter 28 Out 2014 13:04:56 BRST

dnl Process this file with autoconf to produce a configure script.

dnl Require minimum autoconf version
AC_PREREQ([2.50])

AC_INIT([SoftGRE],
        [1.0rc1],
        [https://github.com/jpereira/softgre/issues],
        [softgred],
        [https://github.com/jpereira/softgre/])

AC_CONFIG_SRCDIR([src])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE 
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign])

dnl
dnl check for programs
dnl
AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_C_O
AC_LANG(C)
GCC_STACK_PROTECTOR

dnl
dnl Default options
dnl
AM_CFLAGS="$AM_CFLAGS -fPIC -W -Wall -rdynamic"

dnl
dnl Checks for typedefs, structures, and compiler characteristics.
dnl
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_STDBOOL
AC_HEADER_STDC

dnl
dnl Checks for library functions.
dnl
AC_FUNC_MALLOC
AC_FUNC_MEMCMP

dnl
dnl checks for typedefs, structures, and compiler characteristics
dnl
AC_TYPE_UINT8_T
AC_TYPE_UINT16_T
AC_TYPE_INT8_T
AC_TYPE_INT16_T
AC_TYPE_INT32_T
AC_TYPE_SIZE_T

dnl
dnl used by backtrace()
dnl
AC_CHECK_HEADERS(execinfo.h)

dnl
dnl checks for header files
dnl
AC_CHECK_HEADERS([stdio.h stddef.h stdbool.h stdlib.h string.h ctype.h stdarg.h pthread.h])

dnl
dnl network headers
dnl
AC_CHECK_HEADERS([netinet/in.h sys/time.h fcntl.h unistd.h sys/socket.h ifaddrs.h])

AC_SEARCH_LIBS([strerror],[cposix])
AC_CHECK_FUNCS([gettimeofday])

dnl check externals
AC_PROG_SED

dnl enable maintainer mode
AM_MAINTAINER_MODE([enable])
PKG_PROG_PKG_CONFIG([0.16])

dnl
dnl Detect GLib
dnl
PKG_CHECK_MODULES(GLIB, glib-2.0 >=  2.32.4, HAVE_GLIB="yes", [
            echo "glib2.0 library is missing. libglib2.0-dev is required for this program"
                        exit -1])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

dnl
dnl Detect pthreads libraries
dnl
PTHREAD_LIBS=error
AC_CHECK_LIB(pthread, pthread_attr_init, PTHREAD_LIBS="-lpthread")
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(pthreads, pthread_attr_init, PTHREAD_LIBS="-lpthreads")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_LIB(c_r, pthread_attr_init, PTHREAD_LIBS="-lc_r")
fi
if test "x$PTHREAD_LIBS" = xerror; then
        AC_CHECK_FUNC(pthread_attr_init, PTHREAD_LIBS="")
fi
AC_SUBST(PTHREAD_LIBS)

dnl
dnl Detect libdhash
dnl
PKG_CHECK_MODULES(DHASH, dhash)
AC_SUBST(DHASH_CFLAGS)
AC_SUBST(DHASH_LIBS)

dnl
dnl Detect libpcap
dnl
AC_CHECK_LIB([pcap],[pcap_create])
AC_CHECK_HEADERS([pcap/pcap.h])

dnl
dnl Detect libnetlink (iproute-dev)
dnl
AC_CHECK_HEADERS([libnetlink.h])
NETLINK_LIBS='-lnetlink'
AC_SUBST([NETLINK_LIBS])

dnl
dnl parameters path
dnl
confdir=$sysconfdir/softgred
logdir=$localstatedir/log
piddir=$localstatedir/run

AC_SUBST(confdir)
AC_SUBST(logdir)
AC_SUBST(piddir)

AC_ARG_WITH(
    confdir,
    AS_HELP_STRING(
    [--with-confdir=DIR],
    [conf dir @<:@SYSCONFDIR/softgred@:>@]
    ),
    [confdir="$withval"]
)

AC_ARG_WITH(
    piddir,
    AS_HELP_STRING(
    [--with-piddir=DIR],
    [pid dir @<:@LOCALSTATEDIR/run@:>@]
    ),
    [piddir="$withval"]
)

AC_ARG_WITH(
    piddir,
    AS_HELP_STRING(
    [--with-piddir=DIR],
    [.pid files @<:@LOCALSTATEDIR/run@:>@]
    ),
    [piddir="$withval"]
)

dnl
dnl debug mode
dnl
AC_ARG_ENABLE([debug],
    [  --enable-debug    Turn on debugging],
        [case "${enableval}" in
            yes) debug=true ;;
            no)  debug=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
        esac],[debug=true])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

dnl
dnl enable devel mode
dnl
AC_ARG_ENABLE([devel],
    [  --enable-devel    Turn on devel mode],
        [case "${enableval}" in
            yes) devel=true ;;
            no)  devel=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-devel]) ;;
        esac],[devel=true])
AM_CONDITIONAL([DEVEL], [test x$devel = xtrue])

dnl
dnl enable service
dnl
AC_ARG_ENABLE([service],
    [  --enable-service    Turn on the SoftGREd Service (web & API)],
        [case "${enableval}" in
            yes) service=true ;;
            no)  service=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-service]) ;;
        esac],[service=true])
AM_CONDITIONAL([SOFTGRED_SERVICE], [test x$service = xtrue])

dnl
dnl that's it!
dnl
AC_CONFIG_FILES([
Makefile
bin/Makefile
share/Makefile
])

AC_OUTPUT
