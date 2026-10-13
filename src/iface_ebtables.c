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

#include "iface_ebtables.h"
#include "helper.h"
/* Dedicated chains: never flush global rules or alter the FORWARD policy. */
static bool initialized;
static const char *ebtables_bin(void)
{
    static char *binary;
    if (!binary) {
        binary = g_find_program_in_path("ebtables-nft");
        if (!binary) binary = g_strdup("ebtables");
    }
    return binary;
}
static int chain_cmd(const char *op, const char *chain)
{
    const char *args[] = { ebtables_bin(), op, chain, NULL };
    return helper_exec(args);
}
static int rule(const char *op, const char *direction, const char *name,
                const char *mac_direction, const char *mac, const char *target)
{
    const char *args[12] = { ebtables_bin(), op, "FORWARD", direction, name };
    unsigned n = 5;
    if (mac) { args[n++] = mac_direction; args[n++] = mac; }
    args[n++] = "-j"; args[n++] = target; args[n] = NULL;
    return helper_exec(args);
}
static int chain_defaults(void)
{
    /* ARP, DHCP and IPv6 discovery need broadcast/multicast toward learned tunnels. */
    const char *multicast[] = {ebtables_bin(), "-A", "SOFTGRE_OUT", "-d",
                               "01:00:00:00:00:00/01:00:00:00:00:00", "-j", "ACCEPT", NULL};
    const char *in[] = {ebtables_bin(), "-A", "SOFTGRE_IN", "-j", "DROP", NULL};
    const char *out[] = {ebtables_bin(), "-A", "SOFTGRE_OUT", "-j", "DROP", NULL};
    return helper_exec(multicast) || helper_exec(in) || helper_exec(out) ? -1 : 0;
}
int iface_ebtables_init(void)
{
    if (chain_cmd("-N", "SOFTGRE_IN")) return -1;
    if (chain_cmd("-N", "SOFTGRE_OUT")) {
        chain_cmd("-X", "SOFTGRE_IN"); return -1;
    }
    initialized = true;
    if (chain_defaults()) { iface_ebtables_end(); return -1; }
    return 0;
}
int iface_ebtables_add_tunnel(const char *name, bool *in, bool *out)
{
    char pattern[IFNAMSIZ + 2];
    snprintf(pattern, sizeof(pattern), "%s.+", name);
    if (rule("-I", "-i", pattern, NULL, NULL, "SOFTGRE_IN")) return -1;
    *in = true;
    if (rule("-I", "-o", pattern, NULL, NULL, "SOFTGRE_OUT")) return -1;
    *out = true;
    return 0;
}
int iface_ebtables_del_tunnel(const char *name, bool *in, bool *out)
{
    char pattern[IFNAMSIZ + 2];
    snprintf(pattern, sizeof(pattern), "%s.+", name);
    if (*in && !rule("-D", "-i", pattern, NULL, NULL, "SOFTGRE_IN")) *in = false;
    if (*out && !rule("-D", "-o", pattern, NULL, NULL, "SOFTGRE_OUT")) *out = false;
    return *in || *out ? -1 : 0;
}
int iface_ebtables_reset(void)
{
    if (chain_cmd("-F", "SOFTGRE_IN") || chain_cmd("-F", "SOFTGRE_OUT")) return -1;
    return chain_defaults();
}
int iface_ebtables_allow(const char *name, const char *mac)
{
    char pattern[IFNAMSIZ + 2];
    snprintf(pattern, sizeof(pattern), "%s.+", name);
    const char *in[] = {ebtables_bin(), "-I", "SOFTGRE_IN", "-i", pattern,
                        "-s", mac, "-j", "ACCEPT", NULL};
    const char *out[] = {ebtables_bin(), "-I", "SOFTGRE_OUT", "-o", pattern,
                         "-d", mac, "-j", "ACCEPT", NULL};
    if (helper_exec(in)) return -1;
    if (helper_exec(out)) {
        const char *undo[] = {ebtables_bin(), "-D", "SOFTGRE_IN", "-i", pattern,
                              "-s", mac, "-j", "ACCEPT", NULL};
        helper_exec(undo); return -1;
    }
    return 0;
}
int iface_ebtables_revoke(const char *name, const char *mac)
{
    char pattern[IFNAMSIZ + 2];
    snprintf(pattern, sizeof(pattern), "%s.+", name);
    const char *in[] = {ebtables_bin(), "-D", "SOFTGRE_IN", "-i", pattern,
                        "-s", mac, "-j", "ACCEPT", NULL};
    const char *out[] = {ebtables_bin(), "-D", "SOFTGRE_OUT", "-o", pattern,
                         "-d", mac, "-j", "ACCEPT", NULL};
    int a = helper_exec(in), b = helper_exec(out);
    return a || b ? -1 : 0;
}
int iface_ebtables_end(void)
{
    if (!initialized) return 0;
    int ret = chain_cmd("-F", "SOFTGRE_IN") | chain_cmd("-F", "SOFTGRE_OUT");
    ret |= chain_cmd("-X", "SOFTGRE_IN") | chain_cmd("-X", "SOFTGRE_OUT");
    if (!ret) initialized = false;
    return ret;
}
