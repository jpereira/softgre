sbin_PROGRAMS = softgred
softgred_SOURCES = ../src/softgred.c ../src/general.h ../src/softgred.h \
    ../src/softgred_config.c ../src/softgred_config.h ../src/log.c ../src/log.h \
    ../src/compat.c ../src/compat.h ../src/hashmap.c ../src/hashmap.h \
    ../src/helper.c ../src/helper.h ../src/payload.c ../src/payload.h \
    ../src/iface_gre.c ../src/iface_gre.h ../src/iface_bridge.c ../src/iface_bridge.h \
    ../src/iface_ebtables.c ../src/iface_ebtables.h ../src/provision.c ../src/provision.h \
    ../src/service.c ../src/service.h ../src/service_cmd.c ../src/service_cmd.h
AM_CPPFLAGS = -I$(top_builddir) -I$(top_srcdir)/src \
    -DSOFTGRED_CONFDIR=\"$(confdir)\" -DSOFTGRED_PIDDIR=\"$(piddir)\"
AM_CFLAGS = -std=c11 -Wall -Wextra -Wformat=2 -Wshadow -Wstrict-prototypes
check_PROGRAMS = test-core test-netlink test-firewall test-helper test-log test-hashmap
test_core_SOURCES = ../tests/test_core.c ../src/payload.c ../src/provision.c \
    ../src/softgred_config.c ../src/service_cmd.c ../src/service.c ../src/log.c ../src/helper.c \
    ../src/compat.c ../src/hashmap.c ../tests/testing.c ../tests/testing.h
test_netlink_SOURCES = ../tests/test_netlink.c ../src/iface_gre.c ../src/compat.c \
    ../tests/testing.c
test_firewall_SOURCES = ../tests/test_firewall.c ../src/iface_ebtables.c ../src/compat.c \
    ../tests/testing.c
test_helper_SOURCES = ../tests/test_helper.c ../src/helper.c ../src/log.c \
    ../src/softgred_config.c ../src/compat.c ../tests/testing.c
TESTS = test-core test-netlink test-firewall test-helper test-log test-hashmap
test_log_SOURCES = ../tests/test_log.c ../src/log.c ../src/softgred_config.c ../src/compat.c \
    ../tests/testing.c
test_hashmap_SOURCES = ../tests/test_hashmap.c ../src/hashmap.c ../src/compat.c \
    ../tests/testing.c ../tests/testing.h
