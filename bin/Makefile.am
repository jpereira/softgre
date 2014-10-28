# Author: Jorge Pereira <jpereiran@gmail.com>
# Created: Mon Sep 22 23:14:38 EDT 2014
# Last Change: Fri 17 Oct 2014 02:53:11 AM BRT
####

DISTCHECK_CONFIGURE_FLAGS = \
	--enable-debug \
	--enable-devel \
	--enable-service \
	--enable-compile-warnings=maximum

SOFTGRED_DEVEL_CFLAGS = -DSOFTGRED_CONFDIR=\"$(confdir)\" \
					    -DSOFTGRED_LOGDIR=\"$(logdir)\"   \
					    -DSOFTGRED_PIDDIR=\"$(piddir)\" 

sbin_PROGRAMS = softgred

if DEVEL
SOFTGRED_DEVEL_CFLAGS += -DDEVEL -g3
endif

if SOFTGRED_SERVICE
SOFTGRED_SERVICE_FILES =      \
	../src/service.c          \
	../src/service.h          \
	../src/service_cmd.c      \
	../src/service_cmd.h
SOFTGRED_SERVICE_CFLAGS = -DSOFTGRED_ENABLE_SERVICE
endif

softgred_SOURCES =            \
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
	../src/provision.h        \
	$(SOFTGRED_SERVICE_FILES)

softgred_CFLAGS =              \
    $(AM_CFLAGS)               \
	-I$(top_srcdir)            \
	-I$(top_srcdir)/src        \
	-I$(top_srcdir)/include    \
	$(SOFTGRED_CFLAGS)         \
	$(WARN_CFLAGS)             \
	$(SOFTGRED_DEVEL_CFLAGS)   \
	$(SOFTGRED_SERVICE_CFLAGS)

softgred_LDFLAGS = $(AM_LIBS)

softgred_LDADD = $(AM_LIBS)      \
                 $(PTHREAD_LIBS) \
                 $(PCAP_LIBS)	 \
				 $(NETLINK_LIBS) \
				 $(DHASH_LIBS)

