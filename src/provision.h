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

#define PROVISION_MAX_SLOTS 1024
#define PROVISION_MAX_SRC   512

struct provision_data {
    int last_slot;
};

struct provision_data *provision_data_get();

struct tunnel_context {
    struct in_addr ip_remote;
    char ifgre[SOFTGRED_MAX_IFACE+1];
    uint16_t id;

    struct {
        struct ether_addr shost;
    } ether[PROVISION_MAX_SRC];
    uint16_t ether_last;
};

struct tunnel_context *provision_has_tunnel(const struct in_addr *ip_remote);

struct tunnel_context *provision_add(const struct in_addr *ip_remote);

int provision_del(const struct in_addr *ip_remote);

void provision_delall();

bool provision_tunnel_has_mac(const struct tunnel_context *tun,
                              const struct ether_addr *ether_shost);

bool provision_tunnel_allow_mac (struct tunnel_context *tun,
                                 const struct ether_addr *ether_shost);


#endif /*PROVISION_H_*/

