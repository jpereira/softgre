/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesse General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU Lesse General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesse General Public License
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
    char ifname[SOFTGRED_MAX_IFACE];
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

#define is_debug(__peace)             (softgred_config_ref()->debug_env.__peace == true)
#define if_debug(__peace, __doit)     if(is_debug(__peace)) { __doit; }

enum data_type {
    T_STRING,
    T_BOOL,
    T_INTEGER,
    T_CALLBACK
};

static const char *
data_type_get_name (enum data_type type)
{
    switch (type)
    {
        case T_STRING: return "string";
        case T_BOOL: return "boolean";
        case T_INTEGER: return "integer";
        case T_CALLBACK: return "callback";
        default: return "unknown";
    }
}

struct softgred_config {
    char *config_file;      /* --config-file */
    bool is_foreground;     /* --foreground */

    // [ global]
    char *ifname;
    char *tunnel_prefix;
    char *pid_file;
    char *log_file;
    bool bridge_force;
    struct tunnel_bridge bridge[SOFTGRED_MAX_ATTACH];
    uint8_t bridge_slot;

    // [service]
    char *srv_bind_in;
    uint16_t srv_port;
    uint32_t srv_max_listen;

    // [debug]
    bool dbg_enable;
    char *dbg_file;
    int32_t dbg_mode;      /* --debug */
    bool dbg_time;         /* --debug-time */

    // TODO: change this
    struct tunnel_context_priv priv;
    struct rtnl_handle rth;
    hash_table_t *table;
    time_t started_time;
    struct utsname uts;

    // TODO: Remover isto breve
    struct {
        bool payload;        /* getenv("SOFTGRED_DEBUG_PAYLOAD") */
        bool cmd;            /* getenv("SOFTGRED_DEBUG_CMD") */
        bool provision;      /* getenv("SOFTGRED_DEBUG_PROVISION") */
        bool service;        /* getenv("SOFTGRED_DEBUG_SERVICE") */
    } debug_env; /* set by softgred_config_load_envs() */
};

struct softgred_config_map {
    const char *group;       /* group name */
    const char *key;         /* config key name */
    bool is_necessary;       /* is necessary? */
    enum data_type type;     /* expected type of value */
    void **ptr;              /* where will save */
};

struct softgred_config *softgred_config_ref();

void softgred_config_release();

int softgred_config_init();

int softgred_config_end();

int softgred_config_load_conf(const char *config_file);

int softgred_config_load_cli(int argc, 
                             char *argv[]);

int softgred_config_create_pid_file(int pid);

void softgred_print_version();

void softgred_print_usage();

#endif /*SOFTGRED_CONFIG_H_*/

