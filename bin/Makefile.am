# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Qui 09 Out 2014 18:52:24 BRT
####

AM_CFLAGS = -W -Wall

sbin_PROGRAMS = softgred

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
				 $(LIBNETLINK_LIBS) \
				 $(LIBDHASH_LIBS)

