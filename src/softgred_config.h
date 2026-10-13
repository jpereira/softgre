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

#ifndef SOFTGRED_CONFIG_H
#define SOFTGRED_CONFIG_H
#include "general.h"

struct tunnel_bridge {
    char ifname[IFNAMSIZ];
    uint16_t vlan_id;
};

struct softgred_config {
    char *config_file, *ifname, *tunnel_prefix, *pid_file;
    char *log_file, *dbg_file, *srv_bind_in;
    bool is_foreground, bridge_force, dbg_enable, dbg_time;
    int dbg_mode;
    unsigned srv_port, srv_max_listen;
    struct tunnel_bridge bridge[SOFTGRED_MAX_ATTACH];
    unsigned bridge_slot;
    struct in_addr local;
    time_t started_time;
    struct utsname uts;
};
struct softgred_config *softgred_config_ref(void);
bool softgred_config_load_conf(const char *path);
int softgred_config_load_cli(int argc, char **argv);
void softgred_config_release(void);
int softgred_config_create_pid_file(int pid);
void softgred_config_remove_pid_file(void);
bool config_valid_ifname(const char *name);
#endif
