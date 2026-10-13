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

#ifndef IFACE_EBTABLES_H
#define IFACE_EBTABLES_H
#include "general.h"
int iface_ebtables_init(void);
int iface_ebtables_add_tunnel(const char *name, bool *in, bool *out);
int iface_ebtables_del_tunnel(const char *name, bool *in, bool *out);
int iface_ebtables_reset(void);
int iface_ebtables_allow(const char *name, const char *mac);
int iface_ebtables_revoke(const char *name, const char *mac);
int iface_ebtables_end(void);
#endif
