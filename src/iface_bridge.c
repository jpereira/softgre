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

#include "iface_bridge.h"

#include "iface_gre.h"
#include "softgred_config.h"

static int set_master(unsigned index, unsigned master)
{
    struct link_request req;
    link_request_init(&req, RTM_NEWLINK, 0, index);
    req.info.ifi_flags = IFF_UP;
    req.info.ifi_change = IFF_UP;
    if (link_attr(&req, IFLA_MASTER, &master, sizeof(master)))
        return -1;
    return link_send(&req);
}

int iface_bridge_prepare(const char *name, bool create)
{
    unsigned index = if_nametoindex(name);
    if (index) {
        char path[128];
        snprintf(path, sizeof(path), "/sys/class/net/%s/bridge", name);
        if (access(path, F_OK)) {
            errno = EINVAL;
            return -1;
        }
    } else {
        if (!create) {
            errno = ENODEV;
            return -1;
        }
        struct link_request req;
        link_request_init(&req, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL, 0);
        req.info.ifi_flags = IFF_UP;
        req.info.ifi_change = IFF_UP;
        if (link_attr(&req, IFLA_IFNAME, name, strlen(name) + 1))
            return -1;
        struct rtattr *info = link_nest(&req, IFLA_LINKINFO);
        if (!info || link_attr(&req, IFLA_INFO_KIND, "bridge", 7))
            return -1;
        link_nest_end(&req, info);
        return link_send(&req);
    }
    struct link_request req;
    link_request_init(&req, RTM_NEWLINK, 0, index);
    req.info.ifi_flags = IFF_UP;
    req.info.ifi_change = IFF_UP;
    return link_send(&req);
}

int iface_bridge_attach(const char *gre, const char *bridge, uint16_t vlan)
{
    char name[IFNAMSIZ];
    int len = snprintf(name, sizeof(name), "%s.%u", gre, vlan);
    uint32_t parent = if_nametoindex(gre), master = if_nametoindex(bridge);
    if (len < 0 || len >= (int)sizeof(name) || !parent || !master) {
        errno = EINVAL;
        return -1;
    }
    struct link_request req;
    link_request_init(&req, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL, 0);
    req.info.ifi_flags = IFF_UP;
    req.info.ifi_change = IFF_UP;
    if (link_attr(&req, IFLA_IFNAME, name, len + 1) ||
        link_attr(&req, IFLA_LINK, &parent, sizeof(parent)))
        return -1;
    struct rtattr *info = link_nest(&req, IFLA_LINKINFO);
    if (!info || link_attr(&req, IFLA_INFO_KIND, "vlan", 5))
        return -1;
    struct rtattr *data = link_nest(&req, IFLA_INFO_DATA);
    if (!data || link_attr(&req, IFLA_VLAN_ID, &vlan, sizeof(vlan)))
        return -1;
    link_nest_end(&req, data);
    link_nest_end(&req, info);
    if (link_send(&req))
        return -1;
    return set_master(if_nametoindex(name), master);
}
