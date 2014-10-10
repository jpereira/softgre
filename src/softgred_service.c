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
#include "softgred_service.h"
#include "log.h"

struct softgred_service *
softgred_service_getref() 
{
    static struct softgred_service ref;
    return &ref;
}

struct connection {
    int fd;
    struct sockaddr_in saddr;
};

struct connection *
connection_new (int sock,
                struct sockaddr_in *saddr)
{
    struct connection *ref = malloc(sizeof(struct connection));

    ref->fd = sock;
    ref->saddr = *saddr; 

    return ref;
}

void
connection_free(struct connection *con)
{
    assert(con != NULL);

    shutdown(con->fd, SHUT_RDWR);
    close(con->fd);

    free (con);
}

int
quote_cb_help(struct connection *conn,
              int argc,
              char *argv[])
{
    char *str = "ainda sem manual";
   
    write(conn->fd, str, strlen(str));

    return 1;
}

struct quote {
    const char *cmd;
    int (*callback)(struct connection *, int, char **);
    int args; /* args expected */
} quotes[] = {
    { "help", quote_cb_help, 0 }
};

bool
quote_run(struct connection *con,
          const char *cmd)
{
    size_t i;

    for (i=0; i < ARRAY_SIZE(quotes); i++)
    {
        struct quote *q = &quotes[i];

        if (!strncmp(cmd, q->cmd, strlen(q->cmd)))
        {
            D_DEBUG3("Calling command %s\n", q->cmd);
            q->callback(con, 0, NULL);
            return true;
        }
    }

    return false;
}

/**
 * here is the heart of SoftGREd/service
 */
static void *
softgred_service_thread_client(void *arg)
{
    struct connection *con = arg;
    const char *host = inet_ntoa(con->saddr.sin_addr);
    char buf[32];
    size_t len;

    D_DEBUG0("[fd=%d] handling connection from %s\n", con->fd, host);

    memset(buf, 0, 32);
    len = read(con->fd, buf, 32);
    D_DEBUG0("buf test data='%s' (len)\n", buf, len);

    if (!quote_run(con, buf))
        dprintf(con->fd, "Command invalid");

    // end connection
    connection_free(con);

    // bye
    pthread_exit(0);
    return NULL;
}

static void *
softgred_service_thread_server(void *UNUSED(arg))
{
    int sock_client = -1, sock_desc = -1;
    struct sockaddr_in s_server = { 0, };
    struct sockaddr_in s_client = { 0, };

    D_INFO("Starting Service!\n");

    // Create socket
    if ((sock_desc = socket(AF_INET, SOCK_STREAM , 0)) < 0)
    {
        D_CRIT("Could not create socket\n");
        return NULL;
    }

    int on = 1;
    setsockopt(sock_desc, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    // Prepare the sockaddr_in structure
    s_server.sin_family = AF_INET;
    s_server.sin_addr.s_addr = INADDR_ANY;
    s_server.sin_port = htons(8888);
     
    // Bind
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
     
    D_INFO("Waiting for incoming connections.\n");
    
    socklen_t len = sizeof(struct sockaddr_in);
    while ((sock_client = accept (sock_desc, (struct sockaddr *)&s_client, &len)) )
    {
        struct connection *conn;
        pthread_t tid;

        D_DEBUG0("Connection accepted\n");

        conn = connection_new (sock_client, &s_client);
        if (pthread_create(&tid, NULL, softgred_service_thread_client, conn) < 0)
        {
            D_CRIT("Problems with pthread_create(), leaving.\n");
            return NULL;
        }
     
        D_DEBUG3("handler assigned for thread %#x\n", tid);

        if (pthread_detach(tid) < 0)
        {
            D_CRIT("Problems with pthread_detach()\n");
        }
    }
     
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
softgred_service_init()
{
    struct softgred_service *srv = softgred_service_getref();

    D_DEBUG0("Initializing SoftGREd Service (socket-file://%s)\n", SOFTGRED_SERVICE_FILESOCK);

    if (pthread_create(&srv->tid, NULL, softgred_service_thread_server, NULL) < 0)
    {
        D_CRIT("Problems with pthread_create(), leaving.\n");
        return -1;
    }
    
    return 0;
}

int
softgred_service_end()
{
    struct softgred_service *srv = softgred_service_getref();

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
softgred_service_stats()
{
    D_INFO("Show stats of SoftGREd/Service (socket-file://%s\n", SOFTGRED_SERVICE_FILESOCK);
}

