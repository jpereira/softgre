sbin_PROGRAMS = softgred
softgred_SOURCES = ../src/softgred.c ../src/general.h ../src/softgred.h \
    ../src/softgred_config.c ../src/softgred_config.h ../src/log.c ../src/log.h \
    ../src/helper.c ../src/helper.h ../src/payload.c ../src/payload.h \
    ../src/iface_gre.c ../src/iface_gre.h ../src/iface_bridge.c ../src/iface_bridge.h \
    ../src/iface_ebtables.c ../src/iface_ebtables.h ../src/provision.c ../src/provision.h \
    ../src/service.c ../src/service.h ../src/service_cmd.c ../src/service_cmd.h
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src $(GLIB_CFLAGS) \
    -DSOFTGRED_CONFDIR=\"$(confdir)\" -DSOFTGRED_PIDDIR=\"$(piddir)\"
AM_CFLAGS = -std=c11 -Wall -Wextra -Wformat=2 -Wshadow -Wstrict-prototypes
softgred_LDADD = $(GLIB_LIBS)
check_PROGRAMS = test-core test-netlink test-firewall test-helper
test_core_SOURCES = ../tests/test_core.c ../src/payload.c ../src/provision.c \
    ../src/softgred_config.c ../src/service_cmd.c ../src/service.c ../src/log.c ../src/helper.c
test_core_LDADD = $(GLIB_LIBS)
test_netlink_SOURCES = ../tests/test_netlink.c ../src/iface_gre.c
test_netlink_LDADD = $(GLIB_LIBS)
test_firewall_SOURCES = ../tests/test_firewall.c ../src/iface_ebtables.c
test_firewall_LDADD = $(GLIB_LIBS)
test_helper_SOURCES = ../tests/test_helper.c ../src/helper.c ../src/log.c \
    ../src/softgred_config.c
test_helper_LDADD = $(GLIB_LIBS)
TESTS = test-core test-netlink test-firewall test-helper
