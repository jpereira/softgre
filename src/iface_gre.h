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

#ifndef IFACE_GRE_H
#define IFACE_GRE_H
#include "general.h"

struct link_request {
    struct nlmsghdr header;
    struct ifinfomsg info;
    unsigned char attrs[1024];
};

void link_request_init(struct link_request *req, int type, unsigned flags, unsigned index);
int link_attr(struct link_request *req, unsigned type, const void *data, size_t len);
struct rtattr *link_nest(struct link_request *req, unsigned type);
void link_nest_end(struct link_request *req, struct rtattr *attr);
int link_send(struct link_request *req);
int iface_gre_add(const char *name, const char *kind, const char *dev, const struct in_addr *local,
                  const struct in_addr *remote);
int iface_gre_del(const char *name);
#endif
