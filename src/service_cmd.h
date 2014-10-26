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

#ifndef SERVICE_CMD_H_
#define SERVICE_CMD_H_

#include "general.h"

#include <glib.h>
#include <glib/gprintf.h>

/**
 *
 */
#define SOFTGRED_REQUEST_MAX_PARAMS     3
#define SOFTGRED_REQUEST_MAX_DATA       64
#define SOFTGRED_REQUEST_BUF            2048
#define RET_OK          "OK"
#define RET_NOTFOUND    "NOTFOUND"
#define RET_INVALID     "INVALID"


struct request {
    int fd; /* socket of client */
    struct sockaddr_in saddr; /* connection information of client */

    char data[SOFTGRED_REQUEST_MAX_DATA]; /* buf used to read/write with client*/
    size_t data_len; /* len of data[] */
    int argc; /* argc & argv, same concept of main(int argc, char *argv[]) */
    char *argv[SOFTGRED_REQUEST_MAX_PARAMS];

    char buf[SOFTGRED_REQUEST_BUF];
    size_t len;
};

struct request *request_new(int sock,
                            struct sockaddr_in *saddr);

int request_append(struct request *req,
                   const char *format,
                   ...);

void request_free(struct request *req);

struct service_cmd {
    const char *cmd;
    int (*callback)(struct request *);
    int max_args; /* args expected */
    const char *syntax;
    const char *desc;
};

bool service_cmd_handler(struct request *con);

/* callback list*/
int cmd_cb_HELP(struct request *req);
int cmd_cb_QUIT(struct request *req);
int cmd_cb_LMIP(struct request *req);
int cmd_cb_GTMC(struct request *req);
int cmd_cb_STAT(struct request *req);
int cmd_cb_STUN(struct request *req);

#endif /*SERVICE_CMD_H_*/

