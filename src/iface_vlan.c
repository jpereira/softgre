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

#define _GNU_SOURCE

#include "general.h"
#include "iface_bridge.h"
#include "log.h"

int
iface_vlan_add(const char *ifname,
               uint16_t vlan_id) {
    struct softgred_config *cfg = softgred_config_ref();
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    int err, master_index;
    char br_ifname[32];

    snprintf(br_ifname, sizeof(br_ifname), "%s.%d", ifname, vlan_id);

    D_DEBUG3("Creating the vlan %s\n", br_ifname);

    cfg->sk = nl_socket_alloc();
    if ((err = nl_connect(cfg->sk, NETLINK_ROUTE)) < 0) {
        D_DEBUG3("Problems with nl_connect()\n");
        return -1;
    }

    if ((err = rtnl_link_alloc_cache(cfg->sk, AF_UNSPEC, &link_cache)) < 0) {
        D_DEBUG3("Problems with rtnl_link_alloc_cache()\n");
        return -1;
    }

    if (!(master_index = rtnl_link_name2i(link_cache, ifname))) {
        D_DEBUG3("Problems with rtnl_link_name2i()\n");
        return -1;
    }

    link = rtnl_link_vlan_alloc();

    rtnl_link_set_link(link, master_index);
    rtnl_link_set_flags(link, IFF_UP);
    rtnl_link_set_name(link, br_ifname);

    rtnl_link_vlan_set_id(link, vlan_id);

    if ((err = rtnl_link_add(cfg->sk, link, NLM_F_CREATE)) < 0) {
        D_DEBUG3("Problems with rtnl_link_add()\n");
        return -1;
    }

    rtnl_link_put(link);
    nl_close(cfg->sk);

    return 1;
}
