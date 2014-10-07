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

struct tunnel_client {
    uint16_t mac;
    tunnel_client *next;
};

struct tunnel_context {
    struct in_addr ip_remote;
    char ifgre[SOFTGRED_MAX_IFACE+1];
    uint16_t id;
};

struct provision_data {
    int last_slot;
};

struct tunnel_context *provision_has_tunnel(const struct in_addr *ip_remote);

struct tunnel_context *provision_add(const struct in_addr *ip_remote);

int provision_del();

int provision_delall();

#endif /*PROVISION_H_*/

