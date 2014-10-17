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

#ifndef SERVICE_H_
#define SERVICE_H_

#include "general.h"

#define SOFTGRED_SERVICE_MAX        10
#define SOFTGRED_SERVICE_FILESOCK   "/tmp/softgred.sock"
#define SOFTGRED_SERVICE_PORT       8888

struct service {
    pthread_t tid;
};

struct service *service_get_ref();

int service_init();

int service_end();

void service_stats();

#endif /*SERVICE_H_*/

