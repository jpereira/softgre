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
#include "iface_gre.h"
#include "log.h"

int
iface_gre_add(const char *gre_iface,
              const char *kind,
              const char *link_iface,
              const struct in_addr *in_local,
              const struct in_addr *in_remote)
{
	int ret = true;
    char *ip_local = strdupa(inet_ntoa(*in_local));
    char *ip_remote = strdupa(inet_ntoa(*in_remote));

    D_DEBUG("Creating gre-interface '%s' { .local='%s', .remote='%s' }\n", 
            gre_iface, ip_local, ip_remote
    );

    return ret;
}

int
iface_gre_del(const char *gre_iface)
{
    return -1;
}

