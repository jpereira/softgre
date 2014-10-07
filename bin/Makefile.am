# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Tue 07 Oct 2014 02:15:34 AM EDT
####

AM_CFLAGS = -W -Wall

if SOFTGRE_HAVE_SELF_IFADDRS
SOFTGRE_IFADDRS_sources = \
	src/helper/ifaddr/getifaddrs.c \
	src/helper/ifaddr/ifaddrs.h
endif

bin_PROGRAMS = softgred

softgred_SOURCES =            \
	$(SOFTGRE_IFADDR_sources) \
	../src/softgred.c         \
	../src/softgred.h         \
	../src/softgred_config.c  \
	../src/softgred_config.h  \
	../src/helper.c           \
	../src/helper.h           \
	../src/log.c              \
	../src/log.h              \
	../src/payload.c          \
	../src/payload.h          \
	../src/iface_bridge.c     \
	../src/iface_bridge.h     \
	../src/iface_ebtables.h   \
	../src/iface_ebtables.c   \
	../src/iface_gre.c        \
	../src/iface_gre.h        \
	../src/provision.c        \
	../src/provision.h 

softgred_CFLAGS =            \
    $(AM_CFLAGS)             \
	-I$(top_srcdir)          \
	-I$(top_srcdir)/src      \
	-I$(top_srcdir)/include  \
	$(SOFTGRED_CFLAGS)       \
	$(WARN_CFLAGS)           \
	$(DISABLE_DEPRECATED) 

softgred_LDFLAGS = $(AM_LIBS)

softgred_LDADD = $(AM_LIBS)         \
                 $(LIBPTHREAD_LIBS) \
                 $(LIBPCAP_LIBS)	\
				 $(LIBNL_LIBS)		\
				 $(LIBNFNETLINK_LIBS) \
				 $(LIBDHASH_LIBS)

if MAINTAINER_MODE
dist-hook: dist-changelog
else
dist-hook:
endif

dist-changelog:
	$(AM_V_at)if git --git-dir=$(top_srcdir)/.git --work-tree=$(top_srcdir) \
		log --no-merges --date=short --pretty='tformat:%cd  %an  <%ae>%n%n%s%n%n%b' master.. | \
	 $(SED)	-e '/^[12]...-[01].-[0123].  [^<>]*  <[^<>]*>$$/,/^$$/ b' \
		-e '/[^	 ]/,/^[	 ]*$$/ !d' \
		-e 's/^[	 ]*/	/' \
		-e 's/^[	 ]*$$//' >.ChangeLog.tmp; \
	then mv -f .ChangeLog.tmp "$(top_distdir)/ChangeLog"; \
	else rm -f .ChangeLog.tmp; exit 1; fi

#dist_noinst_SCRIPTS = autogen.sh

#CLEANFILES = 

DISTCLEANFILES = \
	intltool-extract \
	intltool-merge \
	intltool-update 

.PHONY: dist-changelog

