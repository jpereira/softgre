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

#ifndef PROVISION_H_
#define PROVISION_H_

#include <stdio.h>
#include <stdio.h>
#include <sys/time.h>
#include <pcap.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PROVISION_MAX_SLOTS     4096        // maximum gre interfaces
#define PROVISION_MAX_CLIENTS   1024        // maximum clients conected
#define PROVISION_MAC_SIZE      (17+1)      // e.g 78:2b:cb:bd:33:18\n

struct provision_data {
    int tunnel_pos;
};

struct provision_data *provision_data_get();

struct tunnel_filter {
    const char *in_face;
    const char *out_face;
    char src_mac[PROVISION_MAC_SIZE];
};

struct tunnel_context {
    struct in_addr ip_remote;
    char ifgre[SOFTGRED_MAX_IFACE+1];
    uint16_t id;
    struct tunnel_filter filter[PROVISION_MAX_CLIENTS];
    uint16_t filter_pos;
};

void provision_stats();

hash_entry_t *tunnel_context_new (const struct in_addr *ip_remote,
                                  uint16_t id,
                                  const char *new_ifgre);

struct tunnel_context *provision_has_tunnel(const struct in_addr *ip_remote,
                                            hash_value_t *out_entry);

struct tunnel_context *provision_add(const struct in_addr *ip_remote);

int provision_del(const struct in_addr *ip_remote);

void provision_delall();

bool provision_tunnel_has_mac(const struct tunnel_context *tun,
                              const char *src_mac);

bool provision_tunnel_allow_mac (const struct tunnel_context *tun,
                                 const char *src_mac);

#endif /*PROVISION_H_*/

