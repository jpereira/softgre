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

