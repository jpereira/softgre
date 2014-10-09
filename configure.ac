# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Qui 09 Out 2014 15:22:31 BRT
####

AC_PREREQ([2.50])

AC_INIT([SoftGRE],
        [0.1],
        [https://github.com/jpereira/softgre/issues],
        [softgre],
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

# enable debug/devel mode
AC_ARG_ENABLE(debug,
              [AS_HELP_STRING([--enable-debug], [enable debugging messages (default enabled)])],
              AC_DEFINE([DEBUG], [0], [debug mode off]),
              AC_DEFINE([DEBUG], [1], [debug mode on]))

AC_ARG_ENABLE(devel,
              [AS_HELP_STRING([--enable-devel], [enable development messages (default enabled)])],
              AC_DEFINE([DEVEL], [0], [devel mode off]),
              AC_DEFINE([DEVEL], [1], [devel mode on]))

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_C_BIGENDIAN
AC_HEADER_STDBOOL
AC_HEADER_STDC

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_MEMCMP

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

# Makefiles
AC_CONFIG_FILES([
Makefile
bin/Makefile
])

AC_OUTPUT
