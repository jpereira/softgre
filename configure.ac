AC_PREREQ([2.69])
AC_INIT([SoftGRE], [1.0rc1], [https://github.com/jpereira/softgre/issues], [softgred],
        [https://github.com/jpereira/softgre/])
AC_CONFIG_SRCDIR([src/softgred.c])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([foreign subdir-objects])
AC_PROG_CC
AC_PROG_SED
AC_USE_SYSTEM_EXTENSIONS
AC_CANONICAL_HOST
AS_CASE([$host_os], [linux*], [], [AC_MSG_ERROR([SoftGRE requires Linux])])
PKG_PROG_PKG_CONFIG
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 2.32])
AC_CHECK_HEADERS([linux/if_packet.h linux/filter.h linux/rtnetlink.h], [],
                [AC_MSG_ERROR([Linux networking headers are required])])
AC_ARG_ENABLE([service], AS_HELP_STRING([--disable-service], [Disable TCP status API]),
              [], [enable_service=yes])
AS_CASE([$enable_service], [yes], [AC_DEFINE([SOFTGRED_ENABLE_SERVICE], [1], [TCP status API])],
        [no], [], [AC_MSG_ERROR([Invalid service option])])
AC_ARG_ENABLE([debug], AS_HELP_STRING([--enable-debug], [Build with debug symbols]),
              [], [enable_debug=no])
AS_IF([test "x$enable_debug" = xyes], [CFLAGS="$CFLAGS -g3"])
AC_ARG_ENABLE([devel], AS_HELP_STRING([--enable-devel], [Enable development diagnostics]))
confdir='${sysconfdir}/softgred'
piddir='${localstatedir}/run'
logdir='${localstatedir}/log'
AC_ARG_WITH([confdir], AS_HELP_STRING([--with-confdir=DIR], [Configuration directory]),
            [confdir=$withval])
AC_ARG_WITH([piddir], AS_HELP_STRING([--with-piddir=DIR], [PID directory]), [piddir=$withval])
AC_SUBST([confdir])
AC_SUBST([piddir])
AC_SUBST([logdir])
AC_CONFIG_FILES([Makefile bin/Makefile share/Makefile])
AC_OUTPUT
