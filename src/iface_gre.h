/**
 * Copyright (C) 2014 Jorge Pereira <jpereiran@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef IFACE_GRE_H_
#define IFACE_GRE_H_

#include "general.h"

int
iface_gre_add(const char *gre_iface,
              const char *kind,
              const char *link_iface,
              const struct in_addr *in_local,
              const struct in_addr *in_remote);

int
iface_gre_del(const char *gre_iface);

#endif /*IFACE_GRE_H_*/

