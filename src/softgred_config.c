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
        .bridge = {
            { NULL, 0 },
        },
        .bridge_slot = 0,
        .priv = {
            .ifname_ip    = { 0, },
            .ifname_saddr = { 0, }
        }
    };

    return &singleton;
}

int
softgred_config_load_iface(const char *ifname,
                           struct softgred_config *cfg)
{
    struct sockaddr_in *sin = &cfg->priv.ifname_saddr;
    struct ifaddrs *ifap, *ifa;
    size_t ifacen = strnlen(ifname, SOFTGRED_MAX_IFACE);

    assert (ifname != NULL);
    assert (cfg != NULL);
    assert (sin != NULL);

    getifaddrs (&ifap);

    for (ifa = ifap; ifa; ifa = ifa->ifa_next)
    {
        if (!strncmp(ifname, ifa->ifa_name, ifacen))
        {
            if (ifa->ifa_addr->sa_family == AF_INET)
            {
                struct sockaddr_in *sa = (struct sockaddr_in *) ifa->ifa_addr;
                char *addr = inet_ntoa(sa->sin_addr);

                strncpy(cfg->priv.ifname_ip, addr, SOFTGRED_MAX_IFACE);
                memcpy(sin, sa, sizeof(struct sockaddr_in));

                break;
            }
        }
    }

    freeifaddrs(ifap);

    return 0;
}

int
softgred_config_load_bridge_and_vlans(const char *arg,
                                      struct softgred_config *cfg)
{
    char *tmp = strdupa(arg);
    const char *vlan_id = strtok(arg, "@");
    const char *br = strtok(NULL, "@");
    uint8_t pos = cfg->bridge_slot;

    //printf("Loading the argument '%s' { .vlan_id='%s', .br_iface='%s'\n", tmp, vlan_id, br);

    if (pos >= SOFTGRED_MAX_ATTACH)
    {
        D_WARNING("The maximum number of slots was reached.\n");
        return 0;
    }

    cfg->bridge[pos].ifname = br;
    cfg->bridge[pos].vlan_id = strtol(vlan_id, NULL, 10);
    cfg->bridge_slot += 1;

    return 1;
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
	printf("Usage: %s -i <ethX> -b <br-gre> [-V 10,11,12] [-p tun_prefi] [-b vlan_id1@bridge0 -b vlan_id2@bridge1 ...] [OPTIONS]\n", argv[0]);
    printf("\n");
	printf("   -f, --foreground        Run in the foreground\n");
	printf("   -i, --iface             Network interface to listen\n");
	printf("   -p, --tunnel-prefix     Prefix name to be used in gre-network-interfaces, default e.g: %sN (N num. of instance)\n", SOFTGRED_TUN_PREFIX);
	printf("   -b, --bridge-to-attach  Name of bridge to be attached, e.g: 10@br-vlan2410\n");
	printf("   -h, --help              Display this help text\n");
	printf("   -d, --debug             Enable debug mode\n");
	printf("   -v, --version           Display the %s version\n", argv[0]);
    printf("\n");
}

