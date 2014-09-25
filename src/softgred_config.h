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

#define SOFTGRED_MAX_IFACE      IFNAMSIZ
#define SOFTGRED_MAX_SLOTS      4096
#define SOFTGRED_TUN_PREFIX     "if_sgre"
#define SOFTGRED_TUN_MTU        1462

struct softgred_config {
    bool is_foreground;        /* --foreground */
    const char *ifname;        /* --iface */
    const char *tunnel_prefix; /* --tunnel-prefix */
    uint16_t maximum_slots;    /* --maximum-slots */
    bool debug_mode;           /* --debug */
    struct {
        char ifname_ip[SOFTGRED_MAX_IFACE+1];
        struct sockaddr_in ifname_saddr;
    } priv;
};

void softgred_config_set (struct softgred_config *config);

struct softgred_config *softgred_config_get();

void softgred_print_version();

void softgred_print_usage(char *argv[]);

#endif /*SOFTGRED_CONFIG_H_*/

