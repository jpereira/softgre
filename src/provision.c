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

#define _GNU_SOURCE

#include "general.h"
#include "log.h"
#include "provision.h"
#include "softgred_config.h"

#include "iface_bridge.h"
#include "iface_gre.h"
#include "iface_ebtables.h"

struct provision_data *
provision_data_get_ref()
{
    static struct provision_data ref;
    return &ref;
}

void
provision_stats()
{
    struct softgred_config *cfg = softgred_config_get_ref();
    hash_statistics_t st;

    D_INFO("HashTable 'tunnel_context'(%p) quantity=%ld\n", cfg->table, hash_count(cfg->table));

    // about tunnels
    if (hash_get_statistics(cfg->table, &st) < 0)
    {
        D_CRIT("Problems with hash_get_statistics()\n");
        return;
    }
    D_INFO("HashTable 'tunnel_context'(%p) stats: { accesses=%ld, collisions=%ld }, table:{ expansions=%ld, contractions=%ld }\n",
                cfg->table, st.hash_accesses, st.hash_collisions, st.table_expansions, st.table_contractions);
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
provision_has_tunnel(const struct in_addr *ip_remote,
                     hash_value_t *out_entry)
{
    struct softgred_config *cfg = softgred_config_get_ref();
    hash_value_t value;
    hash_key_t key = { .type = HASH_KEY_ULONG, .ul = ip_remote->s_addr };
    int error = 0;

    assert (cfg->table != NULL);
    assert (ip_remote != NULL);
    assert (cfg->table != NULL);

    if ((error = hash_lookup(cfg->table, &key, &value)) != HASH_SUCCESS)
    {
        if (error != HASH_ERROR_KEY_NOT_FOUND)
            D_CRIT("Problems with hash_lookup(), error=%d(%s)\n", error, hash_error_string(error));

        return NULL;
    }

    assert (value.type != HASH_VALUE_ULONG);

    helper_lock();

    if (out_entry != NULL)
        *out_entry = value;

    helper_unlock();

    return value.ptr;
}

struct tunnel_context *
provision_add(const struct in_addr *ip_remote)
{
    struct provision_data *p = provision_data_get_ref();
    struct softgred_config *cfg = softgred_config_get_ref();
    struct in_addr *ip_local = &cfg->priv.ifname_saddr.sin_addr;
    char new_ifgre[SOFTGRED_MAX_IFACE+1];
    size_t size_new_ifgre;
    int ret;
    int pos = p->tunnel_pos;
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
        fprintf(stderr, "cannot add to table \"%lu\" (%s)\n", entry->key.ul, hash_error_string(error));
        return NULL;
    }

    p->tunnel_pos += 1;
    return entry->value.ptr;
}

int
provision_del(const struct in_addr *ip_remote)
{
    struct tunnel_context *tun = provision_has_tunnel(ip_remote, NULL);

    if (!tun)
        return -1;

    D_DEBUG1("unattach client %s from %s\n", inet_ntoa(tun->ip_remote), tun->ifgre);

    if (!iface_gre_del(tun->ifgre))
    {
        D_WARNING("Problems with iface_gre_del('%s'), continue...\n", tun->ifgre);
        return -1;
    }

    return 0;
}

void
provision_delall()
{
    struct provision_data *p = provision_data_get_ref();
    struct softgred_config *cfg = softgred_config_get_ref();
    struct hash_iter_context_t *iter;
    hash_entry_t *entry;

    assert(cfg->table != NULL);

    if (hash_count(cfg->table) < 1)
    {
        D_WARNING("Don't have any tunnel to unattached!\n");
        return;
    }

    helper_lock();
    iter = new_hash_iter_context(cfg->table);
    while ((entry = iter->next(iter)) != NULL)
    {
        struct tunnel_context *tun = entry->value.ptr;

        D_DEBUG1("unattach client %s from %s\n", inet_ntoa(tun->ip_remote), tun->ifgre);
        if (!iface_gre_del(tun->ifgre))
        {
            D_WARNING("Problems with iface_gre_del('%s'), continue...\n", tun->ifgre);
        }

        p->tunnel_pos -= 1;
        free(tun);

        // removing key
        hash_delete(cfg->table, &entry->key);
    }
    free(iter);
    helper_unlock();
}

bool
provision_tunnel_has_mac(const struct tunnel_context *tun,
                         const char *src_mac)
{
    int i = 0;

    assert (tun != NULL);
    assert (src_mac != NULL);

    for (; i < PROVISION_MAX_CLIENTS; i++)
    {
        const char *cur = tun->filter[i].src_mac;

        if_debug(provision, D_DEBUG3("Checking if (src_mac=['%s'] == cur['%s'])\n", src_mac, cur));

        if (!strncmp(src_mac, cur, strnlen(cur, PROVISION_MAC_SIZE)))
            return true;
    }

    return false;
}

bool
provision_tunnel_allow_mac(const struct tunnel_context *tun,
                           const char *src_mac)
{
    uint16_t *pos = (uint16_t *)&tun->filter_pos;
    struct tunnel_filter *filter = (struct tunnel_filter *)&tun->filter[*pos];

    assert(tun != NULL);
    assert(src_mac != NULL);

    if ((tun->filter_pos + 1) > PROVISION_MAX_CLIENTS)
    {
        D_CRIT("The maximum (%ld) slots was reached\n", PROVISION_MAX_CLIENTS);
        return false;
    }

    helper_lock();
    // saving rules
    *pos += 1;
    strncpy(filter->src_mac, src_mac, strnlen(src_mac, PROVISION_MAC_SIZE));

    // apply
    iface_ebtables_set ("ACCEPT", "FORWARD", tun->ifgre, src_mac);
    helper_unlock();

    return true;
}

struct tunnel_context *
provision_has_tunnel_by_mac(const char *src_mac)
{
    struct softgred_config *cfg = softgred_config_get_ref();
    struct hash_iter_context_t *iter;
    struct tunnel_context *tun = NULL;
    hash_entry_t *entry;

    assert(cfg->table != NULL);

    if (hash_count(cfg->table) < 1)
    {
        D_WARNING("Don't have any tunnel to unattached!\n");
        return NULL;
    }

    helper_lock();
    iter = new_hash_iter_context(cfg->table);
    while ((entry = iter->next(iter)) != NULL)
    {
        tun = entry->value.ptr;

        if (provision_tunnel_has_mac(tun, src_mac))
            break;
    }
    free(iter);
    helper_unlock();

    return tun;
}

