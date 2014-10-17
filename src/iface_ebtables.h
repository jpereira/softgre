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

#ifndef IFACE_EBTABLES_H_
#define IFACE_EBTABLES_H_

#ifndef EBTABLES_BIN
#   define EBTABLES_BIN "/sbin/ebtables"
#endif /*EBTABLES_BIN*/

#include "general.h"

/**
 * ebtables  is an application program used to set up and maintain the tables of rules (inside the
 * Linux kernel) that inspect Ethernet frames.  It is analogous to the iptables application, but less
 * complicated, due to the fact that the Ethernet protocol is much simpler than the IP protocol.
 */

int iface_ebtables_init ();

int iface_ebtables_set (const char *target,
                        const char *chain,
                        const char *in_face,
                        const char *src_mac);

int iface_ebtables_end ();

#endif /*IFACE_EBTABLES_H_*/

