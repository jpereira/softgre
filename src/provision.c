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

#include "provision.h"

#include "iface_bridge.h"
#include "iface_ebtables.h"
#include "iface_gre.h"
#include "log.h"
#include "softgred_config.h"

static struct hashmap *tunnels, *mac_index;
static bool slots[SOFTGRED_MAX_SLOTS];
static uint64_t packets, ignored, failures;
static int64_t retry_after;
static bool cleanup_pending;

struct hashmap *provision_table(void)
{
    return tunnels;
}

static void tunnel_free(void *value)
{
    struct tunnel_context *tun = value;
    hashmap_destroy(tun->macs);
    free(tun);
}

int provision_init(void)
{
    tunnels = hashmap_new(hashmap_hash_u32, hashmap_equal_u32, NULL, tunnel_free);
    mac_index = hashmap_new(hashmap_hash_str, hashmap_equal_str, free, NULL);
    memset(slots, 0, sizeof(slots));
    cleanup_pending = false;
    return 0;
}

struct tunnel_context *provision_get_tunnel_byip(const struct in_addr *remote)
{
    return tunnels ? hashmap_lookup(tunnels, (void *)(uintptr_t)remote->s_addr) : NULL;
}

struct tunnel_context *provision_get_tunnel_by_mac(const char *mac)
{
    return mac_index ? hashmap_lookup(mac_index, mac) : NULL;
}

struct tunnel_context *provision_add(const struct in_addr *remote)
{
    struct softgred_config *cfg = softgred_config_ref();
    if (cleanup_pending)
        return NULL;
    struct tunnel_context *existing = provision_get_tunnel_byip(remote);
    if (existing) {
        if (existing->ready)
            return existing;
        if (provision_del(remote))
            return NULL;
    }
    unsigned id;
    for (id = 0; id < SOFTGRED_MAX_SLOTS; id++) {
        if (slots[id])
            continue;
        char name[IFNAMSIZ];
        snprintf(name, sizeof(name), "%s%u", cfg->tunnel_prefix, id);
        if (!if_nametoindex(name))
            break;
    }
    if (id == SOFTGRED_MAX_SLOTS) {
        errno = ENOSPC;
        return NULL;
    }
    struct tunnel_context *tun = xcalloc(1, sizeof(*tun));
    tun->ip_remote = *remote;
    tun->id = id;
    snprintf(tun->ifgre, sizeof(tun->ifgre), "%s%u", cfg->tunnel_prefix, id);
    tun->macs = hashmap_new(hashmap_hash_str, hashmap_equal_str, free, NULL);
    char ip[INET_ADDRSTRLEN], local[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, remote, ip, sizeof(ip));
    inet_ntop(AF_INET, &cfg->local, local, sizeof(local));
    if (iface_gre_add(tun->ifgre, "gretap", cfg->ifname, &cfg->local, remote)) {
        tunnel_free(tun);
        return NULL;
    }
    D_INFO("Tunnel created remote=%s local=%s interface=%s ingress=%s\n", ip, local, tun->ifgre,
           cfg->ifname);
    /* Track immediately so a failed rollback remains visible and can be retried. */
    slots[id] = true;
    hashmap_insert(tunnels, (void *)(uintptr_t)remote->s_addr, tun);
    if (iface_ebtables_add_tunnel(tun->ifgre, &tun->firewall_in, &tun->firewall_out))
        goto fail;
    for (unsigned i = 0; i < cfg->bridge_slot; i++) {
        if (iface_bridge_attach(tun->ifgre, cfg->bridge[i].ifname, cfg->bridge[i].vlan_id))
            goto fail_firewall;
        D_INFO("VLAN attached remote=%s interface=%s.%u vlan=%u bridge=%s\n", ip, tun->ifgre,
               cfg->bridge[i].vlan_id, cfg->bridge[i].vlan_id, cfg->bridge[i].ifname);
    }
    D_INFO("Tunnel ready remote=%s interface=%s\n", ip, tun->ifgre);
    tun->ready = true;
    return tun;
fail_firewall:
fail:
    provision_del(remote);
    return NULL;
}

bool provision_tunnel_allow_mac(struct tunnel_context *tun, const char *mac)
{
    if (!tun->ready)
        return false;
    if (hashmap_contains(tun->macs, mac))
        return true;
    if (hashmap_size(tun->macs) >= PROVISION_MAX_CLIENTS) {
        errno = ENOSPC;
        return false;
    }
    /* A MAC belongs to one remote; implicit migration would allow takeover. */
    if (provision_get_tunnel_by_mac(mac))
        return false;
    if (iface_ebtables_allow(tun->ifgre, mac))
        return false;
    hashmap_insert(tun->macs, xstrdup(mac), NULL);
    hashmap_insert(mac_index, xstrdup(mac), tun);
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
    D_INFO("Client learned remote=%s mac=%s tunnel=%s clients=%u\n", ip, mac, tun->ifgre,
           (unsigned)hashmap_size(tun->macs));
    return true;
}

void provision_packet(const struct payload_packet *packet)
{
    struct softgred_config *cfg = softgred_config_ref();
    packets++;
    if (cleanup_pending) {
        ignored++;
        return;
    }
    uint32_t host = ntohl(packet->remote.s_addr);
    if (packet->local.s_addr != cfg->local.s_addr || packet->remote.s_addr == cfg->local.s_addr ||
        !host || (host >> 24) == 127 || (host >> 24) == 0 || (host >> 28) >= 14) {
        ignored++;
        return;
    }
    /* Optional ingress authorization: only configured remote networks may provision. This
     * is the defense against spoofed-source GRE claiming MACs or exhausting resources. */
    if (!config_remote_allowed(host)) {
        ignored++;
        return;
    }
    bool mapped = false;
    for (unsigned i = 0; i < cfg->bridge_slot; i++)
        if (cfg->bridge[i].vlan_id == packet->vlan)
            mapped = true;
    if (!mapped) {
        ignored++;
        return;
    }
    char mac[18];
    snprintf(mac, sizeof(mac), "%02x:%02x:%02x:%02x:%02x:%02x", packet->mac[0], packet->mac[1],
             packet->mac[2], packet->mac[3], packet->mac[4], packet->mac[5]);
    struct tunnel_context *tun = provision_get_tunnel_byip(&packet->remote);
    if (tun && hashmap_contains(tun->macs, mac))
        return;
    if (provision_get_tunnel_by_mac(mac)) {
        ignored++;
        return;
    }
    /* Prevent process-spawn / netlink storms when the kernel or firewall rejects work. */
    int64_t now = monotonic_usec();
    if (now < retry_after)
        return;
    errno = 0;
    if (!tun || !tun->ready)
        tun = provision_add(&packet->remote);
    if (tun && tun->ready && provision_tunnel_allow_mac(tun, mac))
        return;
    /* A full tunnel or a full slot table is an expected, remotely triggerable capacity
     * limit: drop the packet. Arming the shared backoff for it would let one peer that
     * fills a tunnel stall provisioning for every other peer. Reserve the backoff for real
     * kernel/firewall errors, which repeated retries would otherwise storm. */
    if (errno == ENOSPC) {
        ignored++;
        return;
    }
    failures++;
    retry_after = now + USEC_PER_SEC;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &packet->remote, ip, sizeof(ip));
    D_WARNING("Provisioning failed remote=%s mac=%s vlan=%u retry=1s\n", ip, mac, packet->vlan);
}

int provision_del(const struct in_addr *remote)
{
    struct tunnel_context *tun = provision_get_tunnel_byip(remote);
    if (!tun)
        return 0;
    if (iface_gre_del(tun->ifgre) && errno != ENODEV)
        return -1;
    struct hashmap_iter iter;
    void *mac;
    hashmap_iter_init(&iter, tun->macs);
    while (hashmap_iter_next(&iter, &mac, NULL)) {
        if (iface_ebtables_revoke(tun->ifgre, mac))
            return -1;
        hashmap_remove(mac_index, mac);
        hashmap_iter_remove(&iter);
    }
    if (iface_ebtables_del_tunnel(tun->ifgre, &tun->firewall_in, &tun->firewall_out))
        return -1;
    char ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, remote, ip, sizeof(ip));
    D_INFO("Tunnel removed remote=%s interface=%s\n", ip, tun->ifgre);
    slots[tun->id] = false;
    hashmap_remove(tunnels, (void *)(uintptr_t)remote->s_addr);
    return 0;
}

void provision_delall(void)
{
    if (!tunnels)
        return;
    cleanup_pending = true;
    bool ok = true;
    struct hashmap_iter iter;
    void *value;
    hashmap_iter_init(&iter, tunnels);
    while (hashmap_iter_next(&iter, NULL, &value)) {
        struct tunnel_context *tun = value;
        tun->ready = false;
        if (iface_gre_del(tun->ifgre) && errno != ENODEV) {
            ok = false;
            continue;
        }
        if (iface_ebtables_del_tunnel(tun->ifgre, &tun->firewall_in, &tun->firewall_out))
            ok = false;
        else {
            char ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
            D_INFO("Tunnel removed remote=%s interface=%s clients=%u\n", ip, tun->ifgre,
                   (unsigned)hashmap_size(tun->macs));
        }
    }
    /* Bulk flush avoids spawning two processes per learned MAC during full cleanup. */
    if (ok && !iface_ebtables_reset()) {
        hashmap_remove_all(mac_index);
        hashmap_remove_all(tunnels);
        memset(slots, 0, sizeof(slots));
        cleanup_pending = false;
        retry_after = 0;
    } else
        D_WARNING("Cleanup incomplete; send SIGUSR1 to retry before learning new peers\n");
}

void provision_end(void)
{
    provision_delall();
    if (mac_index)
        hashmap_destroy(mac_index);
    if (tunnels)
        hashmap_destroy(tunnels);
    mac_index = tunnels = NULL;
}

void provision_stats(void)
{
    D_INFO("Tunnels=%u MACs=%u packets=%" PRIu64 " ignored=%" PRIu64 " failures=%" PRIu64 "\n",
           tunnels ? (unsigned)hashmap_size(tunnels) : 0,
           mac_index ? (unsigned)hashmap_size(mac_index) : 0, packets, ignored, failures);
}
