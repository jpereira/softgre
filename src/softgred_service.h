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

#ifndef IFACE_SERVICE_H_
#define IFACE_SERVICE_H_

#include "general.h"

#define SOFTGRED_SERVICE_MAX      10
#define SOFTGRED_SERVICE_FILESOCK "/tmp/softgred.sock"

struct softgred_service {
    pthread_t tid;
};

struct softgred_service *softgred_service_getref();

int softgred_service_init();

int softgred_service_end();

void softgred_service_stats();

#endif /*IFACE_SERVICE_H_*/

