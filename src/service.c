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

#define _GNU_SOURCE

#include "general.h"
#include "service.h"
#include "service_cmd.h"
#include "log.h"

struct service *
service_ref() 
{
    static struct service ref;
    return &ref;
}

/**
 * here is the heart of SoftGREd/service
 */
static void *
thread_handler(void *arg)
{
    struct request *req = arg;

#if 0
    const char *host = inet_ntoa(req->saddr.sin_addr);

    D_DEBUG0("[fd=%d] handling request from %s\n", req->fd, host);

    int i = 0;
    for (i=0; i < req->argc; i++)
        D_DEBUG2("argv[%d]='%s'\n", i, req->argv[i]);
#endif

    if (!service_cmd_handler(req))
        request_appendf(req, "The command '%s' is invalid, try 'HELP'\n", req->argv[0]);

    // end request
    if (req)
        request_free(req);

    // bye
    pthread_exit(0);
    return NULL;
}

static void *
thread_service(void *UNUSED(arg))
{
    int sock_client = -1, sock_desc = -1, on = 1;
    struct sockaddr_in s_server = { 0, };
    struct sockaddr_in s_client = { 0, };
    struct softgred_config *cfg = softgred_config_ref();
    struct ifreq ifr;

    D_INFO("Starting Service!\n");

    // Create socket
    if ((sock_desc = socket(AF_INET, SOCK_STREAM , 0)) < 0)
    {
        D_CRIT("Could not create socket\n");
        return NULL;
    }

    setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Prepare the sockaddr_in structure
    if (!strcmp(cfg->srv_bind_in, "any"))
    {
        s_server.sin_addr.s_addr = INADDR_ANY;
    }
    else
    {
        memset(&ifr, 0, sizeof(ifr));
        snprintf(ifr.ifr_name, sizeof(ifr.ifr_name), "%s", cfg->srv_bind_in);
        if (setsockopt(sock_desc, SOL_SOCKET, SO_BINDTODEVICE, (void *)&ifr, sizeof(ifr)) < 0)
        {
            D_CRIT("Could not create socket\n");
            return NULL;
        }
    }
     
    // Bind
    s_server.sin_family = AF_INET;
    s_server.sin_port = htons(cfg->srv_port);
    if (bind(sock_desc, (struct sockaddr *)&s_server , sizeof(s_server)) < 0)
    {
        D_CRIT("Problems with bind(), errno=%d (%s)\n", errno, strerror(errno));
        return NULL;
    }
     
    // Listen
    if (listen(sock_desc, 10) < 0)
    {
        D_CRIT("Problems with listen(), errno=%d (%s)\n", errno, strerror(errno));
        return NULL;
    }
     
    D_INFO("Waiting for incoming requests.\n");
    
    socklen_t len = sizeof(struct sockaddr_in);
    while ((sock_client = accept (sock_desc, (struct sockaddr *)&s_client, &len)) )
    {
        struct request *req;
        pthread_t tid;

        req = request_new (sock_client, &s_client);

        if (!req)
            continue;

        if (pthread_create(&tid, NULL, thread_handler, req) < 0)
        {
            D_CRIT("Problems with pthread_create(), leaving.\n");
            return NULL;
        }
     
        //D_DEBUG3("handler assigned for thread %#x\n", tid);

        if (pthread_detach(tid) < 0)
        {
            D_CRIT("Problems with pthread_detach()\n");
        }
    }
    
    shutdown(sock_desc, SHUT_RDWR);
    close (sock_desc);

    if (sock_client < 0)
    {
        D_CRIT("accept() failed\n");
        return NULL;
    }

    D_INFO("exiting...\n");

    pthread_exit(NULL);
    return NULL;
}

int
service_init()
{
    struct service *srv = service_ref();
    struct softgred_config *cfg = softgred_config_ref();

    // if empty, set default values!
    if (!cfg->srv_bind_in)
        cfg->srv_bind_in = strdup(SOFTGRED_DEFAULT_SERVICE_IFACE);

    if (cfg->srv_port == 0)
        cfg->srv_port = SOFTGRED_DEFAULT_SERVICE_PORT;

    if (cfg->srv_max_listen == 0)
        cfg->srv_max_listen = SOFTGRED_DEFAULT_SERVICE_MAX;

    D_DEBUG0("Listening \"SoftGREd Service\", listening in %s:%d with max %d accepts\n",
                    cfg->srv_bind_in, cfg->srv_port, cfg->srv_max_listen
    );

    if (pthread_create(&srv->tid, NULL, thread_service, NULL) < 0)
    {
        D_CRIT("Problems with pthread_create(), leaving.\n");
        return -1;
    }
    
    return 0;
}

int
service_end()
{
    struct service *srv = service_ref();

    D_DEBUG0("Unitializing SoftGREd Service\n");

    pthread_cancel(srv->tid);

    if (pthread_detach(srv->tid) < 0)
    {
        D_CRIT("Problems with pthread_detach(), leaving.\n");
        return -1;
    }
 
    return 0;
}

void
service_stats()
{
    struct softgred_config *cfg = softgred_config_ref();

    D_INFO("Show stats of SoftGREd/Service interface=%s port=%d max_listen=%d\n",
            cfg->srv_bind_in, cfg->srv_port, cfg->srv_max_listen
    );
}

