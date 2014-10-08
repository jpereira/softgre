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

