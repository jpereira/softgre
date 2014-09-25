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

#ifndef IFACE_BRIDGE_H_
#define IFACE_BRIDGE_H_

#include "general.h"

int
iface_bridge_attach(const char *gre_iface,
                    const char *br_ifname,
                    uint16_t vlan_id);

#endif /*IFACE_BRIDGE_H_*/

