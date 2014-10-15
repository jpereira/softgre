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
#include "service_cmd.h"
#include "log.h"
#include "provision.h"

static struct service_cmd service_cmd_list[] = {
    { "HELP", cmd_cb_HELP, 0, NULL,              "show list of commands" },
    { "QUIT", cmd_cb_QUIT, 0, NULL,              "quit connection"},
    { "LMIP", cmd_cb_LMIP, 1, "<ip of cpe>",     "list macs by IP of CPE"},
    { "GTMC", cmd_cb_GTMC, 1, "<mac of client>", "get tunnel infos by MAC of client, result: $iface, $ip_remote"},
    { "STUN", cmd_cb_STUN, 0, NULL,              "show list with all provisioning, result: $ip_remote1@$iface1;$ip_remoteN@$ifaceN;... "},
    { "STAT", cmd_cb_STAT, 0, NULL,              "show status of SoftGREd"},
};

int
cmd_cb_HELP(struct request *req)
{
    size_t i;

    dprintf(req->fd, "Usage: <command> [args1] [args2] ... \n\n");

    for (i=0; i < ARRAY_SIZE(service_cmd_list); i++)
    {
        struct service_cmd *q = &service_cmd_list[i];

        if (!strncmp(req->argv[0], q->cmd, strlen(q->cmd)))
            continue;

        if (q->syntax)
        {
            dprintf(req->fd, "%s - desc: %s, syntax: %s %s\n", q->cmd, q->desc, q->cmd, q->syntax);
        }
        else
        {
            dprintf(req->fd, "%s - desc: %s\n", q->cmd, q->desc);
        }
    }

    dprintf(req->fd, "\n");
    return 0;
}

int
cmd_cb_QUIT(struct request *req)
{
    assert(req != NULL);

    shutdown(req->fd, SHUT_RDWR);
    close(req->fd);

    return 0;
}

int
cmd_cb_LMIP(struct request *req)
{
    struct tunnel_context *tun;
    size_t i = 0;
    struct in_addr addr;

    assert (req != NULL);

    D_DEBUG2("Checking macs from cpe '%s'\n", req->argv[1]);

    addr.s_addr = inet_addr(req->argv[1]);

    tun = provision_get_tunnel_byip(&addr, NULL);
    if (!tun)
    {
        dprintf(req->fd, "RESULT: NOTFOUND\n");
        return 0;
    }

    helper_lock();
    dprintf(req->fd, "RESULT: OK\nBODY: ");
    for (; i < PROVISION_MAX_CLIENTS; i++)
    {
        const char *cur = tun->filter[i].src_mac;

        if (cur[0] != '\0')
            dprintf(req->fd, "%s;", cur);
    }
    helper_unlock();

    return 0;
}

int
cmd_cb_GTMC(struct request *req)
{
    struct tunnel_context *tun;

    assert (req != NULL);

    D_DEBUG2("Checking CPE by MAC '%s'\n", req->argv[1]);

    tun = provision_get_tunnel_by_mac(req->argv[1]);
    if (!tun)
    {
        dprintf(req->fd, "RESULT: NOTFOUND\n");
        return 0;
    }

    const char *ip_remote = inet_ntoa(tun->ip_remote);
    dprintf(req->fd, "RESULT: OK\nBODY: %s, %s\n", ip_remote, tun->ifgre);

    return 0;
}

int
cmd_cb_STAT(struct request *req)
{
    assert(req != NULL);

    dprintf(req->fd, "STAT: OK\nBODY: var=data\n");

    return 0;
}

int
cmd_cb_STUN(struct request *req)
{
    struct tunnel_context **tuns = NULL;
    uint64_t tuns_len = NULL;
    uint64_t i = 0;

    assert (req != NULL);

    D_DEBUG2("Return list with all GRE tunnels.\n");

    if (tunnel_context_get_all(&tuns, &tuns_len) != 0)
    {
        D_CRIT("Problems with tunnel_context_get_all()\n");
    }

    if (tuns_len < 1)
    {
        dprintf(req->fd, "RESULT: NULL\n");
        return 0;
    }

    dprintf(req->fd, "RESULT: OK\nBODY: ");
    for (i=0; i < tuns_len; i++)
    {
        struct tunnel_context *tun = tuns[i];
        const char *ip_remote = inet_ntoa(tun->ip_remote);

        dprintf(req->fd, "%s@%s;", ip_remote, tun->ifgre);
    }

    dprintf(req->fd, "\n");

    free (tuns);

    return 0;
}

bool
service_cmd_handler(struct request *req)
{
    size_t i;

    for (i=0; i < ARRAY_SIZE(service_cmd_list); i++)
    {
        struct service_cmd *q = &service_cmd_list[i];

        if (strncmp(req->argv[0], q->cmd, strlen(q->cmd)))
            continue;

        D_DEBUG3("Calling command '%s' with %d arguments, expected is %d (%s)\n", q->cmd, 
                    req->argc - 1, q->max_args, (req->argc-1 != q->max_args) ? "bad args" : "ok");

        if ((req->argc-1) != q->max_args)
        {
            dprintf(req->fd, "RESULT: INVALID\nBODY: syntax of \"%s\" is \"%s\"\n", q->cmd, q->syntax);
            return true;
        }

        q->callback(req);
        return true;
    }

    return false;
}

struct request *
request_new (int sock,
             struct sockaddr_in *saddr)
{
    struct request *ref = calloc(1, sizeof(struct request));
    char *arg;

    ref->fd = sock;
    ref->saddr = *saddr; 

    D_DEBUG3("request accepted from %s\n", inet_ntoa(ref->saddr.sin_addr));

    /* read the data */
    if ((ref->data_len = read(ref->fd, ref->data, sizeof(ref->data))) < 1)
    {
        free (ref);
        return NULL;
    }

    /* parser the commands */
    for(arg = strtok(ref->data, " ");
        arg != NULL && ref->argc < SOFTGRED_REQUEST_MAX_PARAMS;
        arg = strtok(NULL, " "))
    {
        if (arg[0] != '\0')
            ref->argv[ref->argc++] = arg;
    }

    /* strip */
    if (ref->argc > 0)
    {
        char *str = ref->argv[ref->argc-1];

        for (; *str; str++)
        {
            if (*str == '\n' || *str == '\r' || *str == '\t')
                *str = '\0';
        }
    }

    return ref;
}

void
request_free(struct request *req)
{
    assert(req != NULL);

    D_DEBUG3("Closing\n");
    shutdown(req->fd, SHUT_RDWR);
    close(req->fd);

    free (req);
    req = NULL;
}

