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

#define _GNU_SOURCE

#include "general.h"
#include "iface_bridge.h"
#include "log.h"

// TODO: rewrite this for use rnetlink API, system is NOT SAFE!
int
iface_bridge_attach(const char *gre_iface,
                    const char *br_ifname,
                    uint16_t vlan_id)
{
    D_DEBUG1("Add vlan '%d' in GRE '%s.%d' and attaching with bridge '%s'\n",
                vlan_id, gre_iface, vlan_id, br_ifname
    );

    // cria bridge
    helper_system(false, "%s addbr \"%s\"", SOFTGRED_CMD_BRCTL, br_ifname);

    // cria vlan
    helper_system(false, "%s link add link \"%s\" name \"%s.%d\" up type vlan id \"%d\"",
            SOFTGRED_CMD_IP, gre_iface, gre_iface, vlan_id, vlan_id);

    // attacha bridge
    helper_system(false, "%s link set dev \"%s.%d\" up master \"%s\"",
            SOFTGRED_CMD_IP, gre_iface, vlan_id, br_ifname);

    return 1;
}

