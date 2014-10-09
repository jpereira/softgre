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
    char cmd[256];

    D_DEBUG1("Add vlan '%d' in GRE '%s.%d' and attaching with bridge '%s'\n",
                vlan_id, gre_iface, vlan_id, br_ifname
    );

    // cria bridge
    snprintf(cmd, sizeof(cmd), "/sbin/brctl addbr \"%s\" 1> /dev/null 2>&1\n", br_ifname);
    //printf("cmd='%s'\n", cmd);    
    system(cmd);

    // cria vlan
    snprintf(cmd, sizeof(cmd), "/bin/ip link add link \"%s\" name \"%s.%d\" up type vlan id \"%d\" 1> /dev/null 2>&1\n",
            gre_iface, gre_iface, vlan_id, vlan_id);
    //printf("cmd='%s'\n", cmd);
    system(cmd);

    // attacha bridge
    snprintf(cmd, sizeof(cmd), "/bin/ip link set dev \"%s.%d\" up master \"%s\" 1> /dev/null 2>&1\n",
            gre_iface, vlan_id, br_ifname);
    //printf("cmd='%s'\n", cmd);
    system(cmd);

    return 1;
}

