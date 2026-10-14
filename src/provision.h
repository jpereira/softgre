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

#ifndef PROVISION_H
#define PROVISION_H
#include "general.h"
#include "hashmap.h"
#include "payload.h"

struct tunnel_context {
    struct in_addr ip_remote;
    char ifgre[IFNAMSIZ];
    unsigned id;
    bool ready, firewall_in, firewall_out;
    struct hashmap *macs;
};

int provision_init(void);
void provision_packet(const struct payload_packet *packet);
struct tunnel_context *provision_add(const struct in_addr *remote);
int provision_del(const struct in_addr *remote);
void provision_delall(void);
void provision_end(void);
void provision_stats(void);
struct hashmap *provision_table(void);
struct tunnel_context *provision_get_tunnel_byip(const struct in_addr *remote);
struct tunnel_context *provision_get_tunnel_by_mac(const char *mac);
bool provision_tunnel_allow_mac(struct tunnel_context *tun, const char *mac);
#endif
