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
    if (strlen(text) != 17)
        return false;
    for (unsigned i = 0; i < 17; i++) {
        if (i % 3 == 2 ? text[i] != ':' : !ascii_isxdigit((unsigned char)text[i]))
            return false;
        normalized[i] = ascii_tolower((unsigned char)text[i]);
    }
    normalized[17] = '\0';
    return true;
}

static int compare_mac(const void *a, const void *b)
{
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

struct strbuf *service_command(const char *command)
{
    struct strbuf *out = strbuf_new(NULL);
    if (strlen(command) >= SERVICE_MAX_REQUEST)
        goto invalid;
    char copy[SERVICE_MAX_REQUEST];
    str_lcpy(copy, command, sizeof(copy));
    const char *argv[2] = {0};
    unsigned argc = 0;
    char *save = NULL;
    for (char *tok = strtok_r(copy, " \t\r\n", &save); tok;
         tok = strtok_r(NULL, " \t\r\n", &save)) {
        if (argc < ARRAY_SIZE(argv))
            argv[argc] = tok;
        argc++;
    }
    if (!argc || argc > 2)
        goto invalid;
    const char *cmd = argv[0];
    if (!strcmp(cmd, "LMIP") || !strcmp(cmd, "GTMC")) {
        if (argc != 2)
            goto invalid;
        struct tunnel_context *tun;
        if (!strcmp(cmd, "LMIP")) {
            struct in_addr addr;
            if (inet_pton(AF_INET, argv[1], &addr) != 1)
                goto invalid;
            tun = provision_get_tunnel_byip(&addr);
            if (tun) {
                strbuf_append(out, "RESULT: OK\nBODY: ");
                size_t count;
                void **macs = hashmap_keys(tun->macs, &count);
                qsort(macs, count, sizeof(*macs), compare_mac);
                for (size_t i = 0; i < count; i++)
                    strbuf_appendf(out, "%s%s", (char *)macs[i], i + 1 < count ? ";" : "\n");
                if (!count)
                    strbuf_append_char(out, '\n');
                free(macs);
            }
        } else {
            char mac[18];
            if (!valid_mac(argv[1], mac))
                goto invalid;
            tun = provision_get_tunnel_by_mac(mac);
            if (tun) {
                char ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
                strbuf_appendf(out, "RESULT: OK\nBODY: %s@%s\n", ip, tun->ifgre);
            }
        }
        if (!tun)
            strbuf_append(out, "RESULT: NOTFOUND\n");
    } else {
        if (argc != 1)
            goto invalid;
        if (!strcmp(cmd, "VERS")) {
            strbuf_appendf(out, "RESULT: OK\nBODY: %s\n", PACKAGE_VERSION);
        } else if (!strcmp(cmd, "QUIT")) {
            /* Connection closes after each response. */
        } else if (!strcmp(cmd, "HELP")) {
            strbuf_append(out, "HELP - command list\nVERS - version\nQUIT - close\n"
                               "LMIP <IPv4> - list MACs\nGTMC <MAC> - find tunnel\n"
                               "STUN - list tunnels\nSTAT - system status\n");
        } else if (!strcmp(cmd, "STAT")) {
            struct softgred_config *cfg = softgred_config_ref();
            struct sysinfo info = {0};
            sysinfo(&info);
            strbuf_appendf(out, "RESULT: OK\nBODY: %s %s;%s;%s;%ld;%s;%ld\n", cfg->uts.sysname,
                           cfg->uts.release, cfg->uts.version, cfg->uts.machine, info.uptime,
                           PACKAGE_VERSION, (long)cfg->started_time);
        } else if (!strcmp(cmd, "STUN")) {
            struct hashmap *table = provision_table();
            if (!table || !hashmap_size(table))
                strbuf_append(out, "RESULT: NULL\n");
            else {
                strbuf_append(out, "RESULT: OK\nBODY: ");
                struct hashmap_iter iter;
                void *value;
                hashmap_iter_init(&iter, table);
                while (hashmap_iter_next(&iter, NULL, &value)) {
                    struct tunnel_context *tun = value;
                    char ip[INET_ADDRSTRLEN];
                    inet_ntop(AF_INET, &tun->ip_remote, ip, sizeof(ip));
                    strbuf_appendf(out, "%s@%s;", ip, tun->ifgre);
                }
                strbuf_append_char(out, '\n');
            }
        } else
            goto invalid;
    }
    return out;
invalid:
    strbuf_assign(out, "RESULT: INVALID\nBODY: Use HELP for command syntax\n");
    return out;
}
