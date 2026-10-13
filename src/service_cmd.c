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

#include "service_cmd.h"
#include "provision.h"
#include "softgred_config.h"
static bool valid_mac(const char *text, char normalized[18])
{
    if (strlen(text) != 17) return false;
    for (unsigned i = 0; i < 17; i++) {
        if (i % 3 == 2 ? text[i] != ':' : !g_ascii_isxdigit(text[i])) return false;
        normalized[i] = g_ascii_tolower(text[i]);
    }
    normalized[17] = '\0';
    return true;
}
GString *service_command(const char *command)
{
    GString *out = g_string_new(NULL);
    if (strlen(command) >= SERVICE_MAX_REQUEST) goto invalid;
    char **tokens = g_strsplit_set(command, " \t\r\n", -1);
    const char *argv[3] = {0};
    unsigned argc = 0;
    for (char **p = tokens; *p; p++) {
        if (!**p) continue;
        if (argc == ARRAY_SIZE(argv)) { argc++; break; }
        argv[argc++] = *p;
    }
    if (!argc || argc > 2) goto invalid_tokens;
    const char *cmd = argv[0];
    if (!strcmp(cmd, "LMIP") || !strcmp(cmd, "GTMC")) {
        if (argc != 2) goto invalid_tokens;
        struct tunnel_context *tun;
        if (!strcmp(cmd, "LMIP")) {
            struct in_addr addr;
            if (inet_pton(AF_INET, argv[1], &addr) != 1) goto invalid_tokens;
            tun = provision_get_tunnel_byip(&addr);
            if (tun) {
                g_string_append(out, "RESULT: OK\nBODY: ");
                GList *macs = g_hash_table_get_keys(tun->macs);
                macs = g_list_sort(macs, (GCompareFunc)strcmp);
                for (GList *p = macs; p; p = p->next)
                    g_string_append_printf(out, "%s%s", (char *)p->data, p->next ? ";" : "\n");
                if (!macs) g_string_append_c(out, '\n');
                g_list_free(macs);
            }
        } else {
            char mac[18];
            if (!valid_mac(argv[1], mac)) goto invalid_tokens;
            tun = provision_get_tunnel_by_mac(mac);
            if (tun) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
                g_string_append_printf(out, "RESULT: OK\nBODY: %s@%s\n", ip, tun->ifgre);
            }
        }
        if (!tun) g_string_append(out, "RESULT: NOTFOUND\n");
    } else {
        if (argc != 1) goto invalid_tokens;
        if (!strcmp(cmd, "VERS")) {
            g_string_append_printf(out, "RESULT: OK\nBODY: %s\n", PACKAGE_VERSION);
        } else if (!strcmp(cmd, "QUIT")) {
            /* Connection closes after each response. */
        } else if (!strcmp(cmd, "HELP")) {
            g_string_append(out, "HELP - command list\nVERS - version\nQUIT - close\n"
                            "LMIP <IPv4> - list MACs\nGTMC <MAC> - find tunnel\n"
                            "STUN - list tunnels\nSTAT - system status\n");
        } else if (!strcmp(cmd, "STAT")) {
            struct softgred_config *cfg = softgred_config_ref();
            struct sysinfo info = {0};
            sysinfo(&info);
            g_string_append_printf(out, "RESULT: OK\nBODY: %s %s;%s;%s;%ld;%s;%ld\n",
                                   cfg->uts.sysname, cfg->uts.release, cfg->uts.version,
                                   cfg->uts.machine, info.uptime, PACKAGE_VERSION,
                                   (long)cfg->started_time);
        } else if (!strcmp(cmd, "STUN")) {
            GHashTable *table = provision_table();
            if (!table || !g_hash_table_size(table)) g_string_append(out, "RESULT: NULL\n");
            else {
                g_string_append(out, "RESULT: OK\nBODY: ");
                GHashTableIter iter; void *value;
                g_hash_table_iter_init(&iter, table);
                while (g_hash_table_iter_next(&iter, NULL, &value)) {
                    struct tunnel_context *tun = value;
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
                    g_string_append_printf(out, "%s@%s;", ip, tun->ifgre);
                }
                g_string_append_c(out, '\n');
            }
        } else goto invalid_tokens;
    }
    g_strfreev(tokens);
    return out;
invalid_tokens:
    g_strfreev(tokens);
invalid:
    g_string_assign(out, "RESULT: INVALID\nBODY: Use HELP for command syntax\n");
    return out;
}
