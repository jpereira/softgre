# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Fri 17 Oct 2014 03:50:26 AM BRT
####

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

# check for programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_LANG(C)
GCC_STACK_PROTECTOR

# Cflags
AM_CFLAGS="$AM_CFLAGS -fPIC -W -Wall -rdynamic"

# ---------------------------------------------------------------------
# Enable options
# ---------------------------------------------------------------------

# debug mode
AC_ARG_ENABLE([debug],
    [  --enable-debug    Turn on debugging],
        [case "${enableval}" in
            yes) debug=true ;;
            no)  debug=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-debug]) ;;
        esac],[debug=true])
AM_CONDITIONAL([DEBUG], [test x$debug = xtrue])

# enable devel mode
AC_ARG_ENABLE([devel],
    [  --enable-devel    Turn on devel mode],
        [case "${enableval}" in
            yes) devel=true ;;
            no)  devel=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-devel]) ;;
        esac],[devel=true])
AM_CONDITIONAL([DEVEL], [test x$devel = xtrue])

# enable service
AC_ARG_ENABLE([service],
    [  --enable-service    Turn on the SoftGREd Service (web & API)],
        [case "${enableval}" in
            yes) service=true ;;
            no)  service=false ;;
            *) AC_MSG_ERROR([bad value ${enableval} for --enable-service]) ;;
        esac],[service=true])
AM_CONDITIONAL([SOFTGRED_SERVICE], [test x$service = xtrue])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_STDBOOL
AC_HEADER_STDC

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP

# used by backtrace()
AC_CHECK_HEADERS(execinfo.h)

# checks for header files
AC_CHECK_HEADERS([stdio.h stddef.h stdbool.h stdlib.h string.h ctype.h stdarg.h pthread.h])

# network headers
AC_CHECK_HEADERS([netinet/in.h sys/time.h fcntl.h unistd.h sys/socket.h ifaddrs.h])

AC_SEARCH_LIBS([strerror],[cposix])
AC_CHECK_FUNCS([gettimeofday])

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

# checking by dhash
PKG_CHECK_MODULES(LIBDHASH, dhash)
AC_SUBST(LIBDHASH_CFLAGS)
AC_SUBST(LIBDHASH_LIBS)

# check for libpcap 
AC_CHECK_LIB([pcap],[pcap_create])
AC_CHECK_HEADERS([pcap/pcap.h])

# check for libnetlink (iproute-dev)
AC_CHECK_HEADERS([libnetlink.h])
LIBNETLINK_LIBS='-lnetlink'
AC_SUBST([LIBNETLINK_LIBS])

# path
confdir=$sysconfdir/softgred
logdir=$localstatedir/log
piddir=$localstatedir/run

# substitute custom variables
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

# Makefiles
AC_CONFIG_FILES([
Makefile
bin/Makefile
share/Makefile
])

AC_OUTPUT
