/**
 * Copyright (C) 2014 Jorge Pereira <jpereiran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define _GNU_SOURCE
#include "general.h"
#include "softgred_config.h"
#include "helper.h"
#include "log.h"

void
softgred_config_set(struct softgred_config *cfg)
{
    struct softgred_config *obj = softgred_config_get ();

    memcpy(obj, cfg, sizeof(struct softgred_config));
}

struct softgred_config *
softgred_config_get()
{
    /* below default settings */
    static struct softgred_config singleton = {
        .is_foreground = false,
        .ifname        = NULL,
        .tunnel_prefix = SOFTGRED_TUN_PREFIX,
        .maximum_slots = SOFTGRED_MAX_SLOTS,
        .debug_mode    = false,
        .priv = {
            .ifname_ip    = { 0, },
            .ifname_saddr = { 0, }
        }
    };

    return &singleton;
}

void
softgred_print_version()
{
	printf("%s By Jorge Pereira <jpereiran@gmail.com>\n", PACKAGE_STRING);
    printf("Project Website: %s\n", PACKAGE_URL);
    printf("Bugreport:       %s\n", PACKAGE_BUGREPORT);
    printf("Latest Build:    %s - %s\n", __TIME__, __DATE__);
}

void
softgred_print_usage(char *argv[])
{
	printf("Usage: %s -i <ethX> -b <br-gre> [-V 10,11,12] [-f] [-p tun_prefi] [OPTIONS]\n", argv[0]);
    printf("\n");
	printf("   -f, --foreground        Run in the foreground\n");
	printf("   -i, --iface             Network interface to listen\n");
	printf("   -p, --tunnel-prefix     Prefix name to be used in gre-network-interfaces, default e.g: %sN (N num. of instance)\n", SOFTGRED_TUN_PREFIX);
	printf("   -b, --bridge-to-attach  Name of bridge to be attached, e.g: br-vlanX\n");
	printf("   -V, --vlans             List of vlans do be joined with e.g: '-b bridge -B 10,11,12'\n");
	printf("   -h, --help              Display this help text\n");
	printf("   -d, --debug             Enable debug mode\n");
	printf("   -v, --version           Display the %s version\n", argv[0]);
    printf("\n");
}

