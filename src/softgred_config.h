/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
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

struct softgred_config_debug_env {
    const char *var;
    bool *flag;
};

#define is_debug(__peace)             (softgred_config_get_ref()->debug_env.__peace == true)
#define if_debug(__peace, __doit)     if(is_debug(__peace)) { __doit; }

struct softgred_config {
    bool is_foreground;        /* --foreground */
    const char *ifname;        /* --iface */
    const char *tunnel_prefix; /* --tunnel-prefix */
    uint8_t debug_mode;        /* --debug */
    bool debug_xmode;          /* --xdebug */
    bool print_time;           /* --print-time */

    struct tunnel_bridge bridge[SOFTGRED_MAX_ATTACH];
    uint8_t bridge_slot;

    struct {
        bool payload;        /* getenv("SOFTGRED_DEBUG_PAYLOAD") */
        bool cmd;            /* getenv("SOFTGRED_DEBUG_CMD") */
        bool provision;      /* getenv("SOFTGRED_DEBUG_PROVISION") */
    } debug_env; /* set by softgred_config_load_envs() */

    struct tunnel_context_priv priv;
    struct rtnl_handle rth;
    hash_table_t *table;
};

struct softgred_config *softgred_config_get_ref();

int softgred_config_init();

int softgred_config_end();

void softgred_config_load_envs();

int softgred_config_load_cli(int argc, 
                             char *argv[]);

int softgred_config_load_iface(const char *ifname,
                               struct softgred_config *cfg);

int softgred_config_load_attach(const char *arg,
                                struct softgred_config *cfg);

void softgred_print_version();

void softgred_print_usage(char *argv[]);

#endif /*SOFTGRED_CONFIG_H_*/

