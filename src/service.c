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

#include "service.h"

#include "log.h"
#include "service_cmd.h"
#include "softgred_config.h"

#ifdef SOFTGRED_ENABLE_SERVICE
static int listener = -1;

struct client {
    int fd;
    char input[SERVICE_MAX_REQUEST];
    size_t used, sent;
    struct strbuf *response;
    int64_t deadline;
};
static struct client clients[SERVICE_MAX_CLIENTS];
static unsigned poll_slots[SERVICE_MAX_CLIENTS];
static unsigned limit;

static void close_client(struct client *client)
{
    if (client->fd >= 0)
        close(client->fd);
    if (client->response)
        strbuf_free(client->response);
    memset(client, 0, sizeof(*client));
    client->fd = -1;
}

int service_init(void)
{
    struct softgred_config *cfg = softgred_config_ref();
    limit = cfg->srv_max_listen;
    for (unsigned i = 0; i < ARRAY_SIZE(clients); i++)
        clients[i].fd = -1;
    listener = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (listener < 0)
        return -1;
    int on = 1;
    setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(cfg->srv_port)};
    if (!strcmp(cfg->srv_bind_in, "lo"))
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    else if (strcmp(cfg->srv_bind_in, "any")) {
        if (!if_nametoindex(cfg->srv_bind_in) ||
            setsockopt(listener, SOL_SOCKET, SO_BINDTODEVICE, cfg->srv_bind_in,
                       strlen(cfg->srv_bind_in) + 1))
            goto fail;
        struct ifaddrs *addresses;
        if (getifaddrs(&addresses))
            goto fail;
        bool found = false;
        for (struct ifaddrs *p = addresses; p; p = p->ifa_next)
            if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET &&
                !strcmp(p->ifa_name, cfg->srv_bind_in)) {
                addr.sin_addr = ((struct sockaddr_in *)p->ifa_addr)->sin_addr;
                found = true;
                break;
            }
        freeifaddrs(addresses);
        if (!found) {
            errno = EADDRNOTAVAIL;
            goto fail;
        }
    }
    if (bind(listener, (struct sockaddr *)&addr, sizeof(addr)) || listen(listener, limit))
        goto fail;
    D_INFO("Status API on %s:%u (limit %u)\n", cfg->srv_bind_in, cfg->srv_port, limit);
    return 0;
fail : {
    int saved = errno;
    close(listener);
    listener = -1;
    errno = saved;
}
    return -1;
}

unsigned service_pollfds(struct pollfd *fds)
{
    if (listener < 0)
        return 0;
    unsigned count = 1;
    fds[0] = (struct pollfd){.fd = listener, .events = POLLIN};
    for (unsigned i = 0; i < limit; i++) {
        if (clients[i].fd < 0)
            continue;
        poll_slots[count - 1] = i;
        fds[count++] =
            (struct pollfd){.fd = clients[i].fd, .events = clients[i].response ? POLLOUT : POLLIN};
    }
    return count;
}

void service_dispatch(struct pollfd *fds, unsigned count)
{
    if (!count)
        return;
    /* Handle only clients represented in this poll, before accepting new ones. */
    int64_t now = monotonic_usec();
    for (unsigned n = 1; n < count; n++) {
        struct client *client = &clients[poll_slots[n - 1]];
        short events = fds[n].revents;
        if (now >= client->deadline || (events & (POLLERR | POLLNVAL))) {
            close_client(client);
            continue;
        }
        if (!client->response && (events & (POLLIN | POLLHUP))) {
            ssize_t got = recv(client->fd, client->input + client->used,
                               sizeof(client->input) - 1 - client->used, 0);
            if (got < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            if (got < 0) {
                close_client(client);
                continue;
            }
            if (got > 0)
                client->used += got;
            client->input[client->used] = '\0';
            if (memchr(client->input, '\0', client->used)) {
                close_client(client);
                continue;
            }
            char *end = memchr(client->input, '\n', client->used);
            if (end || got == 0 || client->used == sizeof(client->input) - 1) {
                if (end)
                    *end = '\0';
                if (!end && client->used == sizeof(client->input) - 1)
                    client->response = strbuf_new("RESULT: INVALID\nBODY: Request too large\n");
                else
                    client->response = service_command(client->input);
                if (client->response->len > SERVICE_MAX_RESPONSE) {
                    strbuf_assign(client->response, "RESULT: INVALID\nBODY: Response too large\n");
                }
            }
        }
        if (client->response && (events & (POLLOUT | POLLHUP))) {
            ssize_t sent = send(client->fd, client->response->str + client->sent,
                                client->response->len - client->sent, MSG_NOSIGNAL);
            if (sent < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            if (sent <= 0) {
                close_client(client);
                continue;
            }
            client->sent += sent;
            if (client->sent == client->response->len)
                close_client(client);
        }
        if (client->response && !client->response->len)
            close_client(client);
    }
    if (fds[0].revents & POLLIN) {
        for (unsigned batch = 0; batch < 16; batch++) {
            int fd = accept4(listener, NULL, NULL, SOCK_NONBLOCK | SOCK_CLOEXEC);
            if (fd < 0)
                break;
            unsigned slot;
            for (slot = 0; slot < limit && clients[slot].fd >= 0; slot++) {
            }
            if (slot == limit) {
                close(fd);
                continue;
            }
            clients[slot].fd = fd;
            clients[slot].deadline = now + 5 * USEC_PER_SEC;
        }
    }
}

void service_end(void)
{
    if (listener < 0)
        return;
    for (unsigned i = 0; i < limit; i++)
        close_client(&clients[i]);
    close(listener);
    listener = -1;
}
#else
int service_init(void)
{
    return 0;
}

unsigned service_pollfds(struct pollfd *fds)
{
    (void)fds;
    return 0;
}

void service_dispatch(struct pollfd *fds, unsigned count)
{
    (void)fds;
    (void)count;
}

void service_end(void)
{
}
#endif
