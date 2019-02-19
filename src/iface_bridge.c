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
iface_bridge_create(const char *br_ifname) {
    struct softgred_config *cfg = softgred_config_ref();
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct rtnl_link *ltap;
    int err;

    D_DEBUG3("Creating the bridge=%s\n", br_ifname);

    // Created the bridge
    cfg->sk = nl_socket_alloc();
    if ((err = nl_connect(cfg->sk, NETLINK_ROUTE)) < 0) {
        D_DEBUG3("Problems with nl_connect()\n");
        return -1;
    }

    if ((err = rtnl_link_alloc_cache(cfg->sk, AF_UNSPEC, &link_cache)) < 0) {
        D_DEBUG3("Problems with rtnl_link_alloc_cache()\n");
        return -1;
    }

    link = rtnl_link_alloc();
    if ((err = rtnl_link_set_type(link, "bridge")) < 0) {
        rtnl_link_put(link);
        D_DEBUG3("Problems with rtnl_link_set_type()\n");
        return -1;
    }

    rtnl_link_set_name(link, br_ifname);
    rtnl_link_set_flags(link, IFF_UP);

    if ((err = rtnl_link_add(cfg->sk, link, NLM_F_CREATE)) < 0) {
        D_DEBUG3("Problems with rtnl_link_add()\n");
        return -1;
    }

    nl_cache_refill(cfg->sk, link_cache);

    rtnl_link_put(link);

    nl_cache_free(link_cache);
    nl_socket_free(cfg->sk);

    return 1;
}

int
iface_bridge_add(const char *br_ifname,
                 const char *ifname) {
    struct softgred_config *cfg = softgred_config_ref();
    struct rtnl_link *link;
    struct nl_cache *link_cache;
    struct nl_sock *sk;
    struct rtnl_link *ltap;
    int err;

    D_DEBUG3("Add %s in %s\n", ifname, br_ifname);

    sk = nl_socket_alloc();
    if ((err = nl_connect(sk, NETLINK_ROUTE)) < 0) {
        nl_perror(err, "Unable to connect socket");
        return err;
    }

    if ((err = rtnl_link_alloc_cache(sk, AF_UNSPEC, &link_cache)) < 0) {
        nl_perror(err, "Unable to allocate cache");
        return err;
    }

    nl_cache_refill(sk, link_cache);

    link = rtnl_link_get_by_name(link_cache, br_ifname);
    ltap = rtnl_link_get_by_name(link_cache, ifname);
    if (!ltap) {
        fprintf(stderr, "You should create a tap interface before lunch this test (# tunctl -t %s)\n", ifname);
        return -1;
    }

    if ((err = rtnl_link_enslave(sk, link, ltap)) < 0) {
        nl_perror(err, "Unable to enslave interface to his bridge\n");
        return err;
    }

    if(rtnl_link_is_bridge(link) == 0) {
        fprintf(stderr, "Link is not a bridge\n");
        return -2;
    }

    rtnl_link_put(ltap);
    nl_cache_refill(sk, link_cache);
    ltap = rtnl_link_get_by_name(link_cache, ifname);

    if(rtnl_link_get_master(ltap) <= 0) {
        fprintf(stderr, "Interface is not attached to a bridge\n");
        return -3;
    }

    rtnl_link_put(ltap);
    rtnl_link_put(link);

    nl_cache_free(link_cache);
    nl_socket_free(sk);

    return 1; 
}
