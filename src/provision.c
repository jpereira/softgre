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
#include "softgred_config.h"

#include "iface_bridge.h"
#include "iface_gre.h"

struct provision_data *
provision_data_get()
{
    static struct provision_data ref;
    return &ref;
}

hash_entry_t *
tunnel_context_new (const struct in_addr *ip_remote,
                   uint16_t id,
                   const char *new_ifgre)
{
    struct tunnel_context *ref;
    static hash_entry_t entry;

    assert (new_ifgre != NULL);
    assert (ip_remote != NULL);

    ref = calloc(1, sizeof(struct tunnel_context));
    if (!ref)
        return NULL;

    entry.key.type = HASH_KEY_ULONG;
    entry.key.ul = ip_remote->s_addr;
    entry.value.type = HASH_VALUE_PTR;
    entry.value.ptr = ref;

    ref->id = id;
    memcpy(&ref->ip_remote, ip_remote, sizeof(struct in_addr));
    strncpy(ref->ifgre, new_ifgre, strlen(new_ifgre));

    return &entry;

}

struct tunnel_context *
provision_has_tunnel(const struct in_addr *ip_remote)
{
    struct softgred_config *cfg = softgred_config_get();
    int error = 0;
    hash_key_t key = { .type = HASH_KEY_ULONG, .ul = ip_remote->s_addr };
    hash_value_t value;

    assert (cfg->table != NULL);
    assert (ip_remote != NULL);

    if ((error = hash_lookup(cfg->table, &key, &value)) != HASH_SUCCESS)
    {
        fprintf(stderr, "cannot get key list (%s)\n", hash_error_string(error));
        return NULL;
    }

    return (struct tunnel_context *)value.ptr;
}

struct tunnel_context *
provision_add(const struct in_addr *ip_remote)
{
    struct provision_data *p = provision_data_get();
    struct softgred_config *cfg = softgred_config_get();
    struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;
    char new_ifgre[SOFTGRED_MAX_IFACE+1];
    size_t size_new_ifgre;
    int ret;
    int pos = p->last_slot;
    int i = 0;

    assert (ip_remote != NULL);
    assert (cfg != NULL);

    if (pos >= PROVISION_MAX_SLOTS)
    {
        D_WARNING("No more slots availables, leaving...\n");
        return NULL;
    }

    size_new_ifgre = snprintf(new_ifgre, SOFTGRED_MAX_IFACE, "%s%d", cfg->tunnel_prefix, pos);
    if (size_new_ifgre < 1)
    {
        D_WARNING("Problems with name of slot[%d], leaving...\n", pos);
        return NULL;
    }

    // Create GRE Interface
    ret = iface_gre_add(new_ifgre, "gretap", cfg->ifname, ip_local, ip_remote);
    if (ret == false)
    {
        D_WARNING("Problems with iface_gre_add()...\n");
        return NULL;
    }

    // Attach the vlan in some bridge interface
    for (i=0; i < cfg->bridge_slot; i++)
    {
        const char *br = cfg->bridge[i].ifname;
        uint16_t vlan_id = cfg->bridge[i].vlan_id;

        if (iface_bridge_attach(new_ifgre, br, vlan_id) == false)
        {
            D_WARNING("Problems with iface_gre_add()...\n");
            return NULL;
        }
    }

    // save the entry
    int error;
    hash_entry_t *entry = tunnel_context_new(ip_remote, pos, new_ifgre);
    if ((error = hash_enter(cfg->table, &entry->key, &entry->value)) != HASH_SUCCESS)
    {
        fprintf(stderr, "cannot add to table \"%#08x\" (%s)\n", entry->key.ul, hash_error_string(error));
        return NULL;
    }

    return entry->value.ptr;
}

int
provision_del()
{
    return 0;
}

int
provision_delall()
{
    struct softgred_config *cfg = softgred_config_get();
    struct hash_iter_context_t *iter;
    hash_entry_t *entry;

    iter = new_hash_iter_context(cfg->table);
    while ((entry = iter->next(iter)) != NULL)
    {
        struct tunnel_context *tun = entry->value.ptr;

        D_DEBUG1("unattach client %s from %s\n", inet_ntoa(tun->ip_remote), tun->ifgre);
        if (!iface_gre_del(tun->ifgre))
        {
            D_WARNING("Problems with iface_gre_del('%s'), continue...\n", tun->ifgre);
        }

        free(tun);
    }
    free(iter);

    return true;
}

