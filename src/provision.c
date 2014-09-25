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
#include "log.h"
#include "provision.h"
#include "iface_gre.h"
#include "softgred_config.h"

struct provision_data *
provision_data_get()
{
    static struct provision_data pdata;
    return &pdata;
}

bool
provision_is_exist(const struct in_addr *ip_remote)
{
    struct provision_data *p = provision_data_get();
    int i = 0;

    assert (ip_remote != NULL);
    assert (p != NULL);

    for (i=0; i < PROVISION_MAX_SLOTS; i++)
    {
        if (ip_remote->s_addr == p->tunnel[i].ip_remote.s_addr)
            return true;
    }

    return false;
}

int
provision_add(const struct in_addr *ip_remote,
              const struct tunnel_config **tun_cfg)
{
    struct provision_data *p = provision_data_get();
    struct softgred_config *cfg = softgred_config_get();
    struct tunnel_config *tunnel;
    struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;
    char new_ifgre[SOFTGRED_MAX_IFACE+1];
    size_t size_new_ifgre;
    int ret;
    int pos = p->last_slot;

    assert (ip_remote != NULL);
    assert (cfg != NULL);
    assert (p != NULL);

    tunnel = &p->tunnel[pos];
    if (pos >= PROVISION_MAX_SLOTS || !tunnel)
    {
        D_WARNING("No more slots availables, leaving...\n");
        return -1;
    }

    size_new_ifgre = snprintf(new_ifgre, SOFTGRED_MAX_IFACE, "%s%d", cfg->tunnel_prefix, pos);
    if (size_new_ifgre < 1)
    {
        D_WARNING("Problems with name of slot[%d], leaving...\n", pos);
        return -1;        
    }

    // Create GRE Interface
    ret = iface_gre_add(new_ifgre, "gretap", cfg->ifname, ip_local, ip_remote);
    if (ret == false)
    {
        D_WARNING("Problems with iface_gre_add()...\n");
        return -1;
    }

    // save the context
    p->tunnel[pos].ip_remote.s_addr = ip_remote->s_addr;
    p->tunnel[pos].id               = pos;
    p->last_slot                    = pos + 1;
    strncpy(p->tunnel[pos].ifgre, new_ifgre, size_new_ifgre);

    // rise from your grave!!
    *tun_cfg = (const struct tunnel_config *)&p->tunnel[pos];

    return true;
}

int
provision_del()
{
    return 0;
}

int provision_delall()
{
    struct provision_data *p = provision_data_get();
    int i = 0;
    char *ifname;
    assert (p != NULL);

    for (i=0; i < PROVISION_MAX_SLOTS; i++)
    {
        ifname = &p->tunnel[i].ifgre[0];

        if (!*ifname)
            continue;

        D_DEBUG("Unprovisioning gre-interface %s\n", ifname);
        if (!iface_gre_del(ifname))
        {
            D_WARNING("Problems for del iface_gre_del('%s'), continue...\n", ifname);
        }
    }

    return true;
}

