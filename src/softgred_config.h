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

#ifndef SOFTGRED_CONFIG_H_
#define SOFTGRED_CONFIG_H_

#include "general.h"
#include "config.h"

#define SOFTGRED_MAX_ATTACH         10
#define SOFTGRED_MAX_IFACE          IFNAMSIZ
#define SOFTGRED_MAX_SLOTS          4096
#define SOFTGRED_TUN_PREFIX         "sgre" 
#define SOFTGRED_TUN_PREFIX_MAX     10
#define SOFTGRED_TUN_MTU            1462

struct tunnel_bridge {
    const char *ifname;
    uint16_t vlan_id;
};

struct tunnel_context_priv {
    char ifname_ip[SOFTGRED_MAX_IFACE+1];
    struct sockaddr_in ifname_saddr;
};

struct softgred_config {
    bool is_foreground;        /* --foreground */
    const char *ifname;        /* --iface */
    const char *tunnel_prefix; /* --tunnel-prefix */
    uint8_t debug_mode;        /* --debug */
    struct tunnel_bridge bridge[SOFTGRED_MAX_ATTACH];
    uint8_t bridge_slot;
    struct tunnel_context_priv priv;

    struct rtnl_handle rth;

    hash_table_t *table;
};

void softgred_config_set (struct softgred_config *config);

struct softgred_config *softgred_config_get();

int softgred_config_init();

int softgred_config_end();

int softgred_config_load_cli(int argc, 
                             char *argv[]);

void softgred_print_version();

void softgred_print_usage(char *argv[]);

int softgred_config_load_iface(const char *ifname,
                               struct softgred_config *cfg);

int softgred_config_load_attach(const char *arg,
                                struct softgred_config *cfg);

void softgred_savepidfile();

#endif /*SOFTGRED_CONFIG_H_*/

