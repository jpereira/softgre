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
#include "iface_gre.h"
#include "log.h"

int
iface_gre_add(const char *gre_iface,
              const char *gre_type,
              const char *dev_to_link,
              const struct in_addr *in_local,
              const struct in_addr *in_remote) {
    struct softgred_config *cfg = softgred_config_ref();
    int ret = false;
    int err, if_index;
    struct rtnl_link *link;
    struct nl_cache *link_cache;

    D_DEBUG1("Creating gre-interface '%s' { .local='%s', .remote='%s' } gre_type=%s\n", 
            gre_iface, inet_ntoa(*in_local), inet_ntoa(*in_remote), gre_type
    );

    cfg->sk = nl_socket_alloc();
    if ((ret = nl_connect(cfg->sk, NETLINK_ROUTE)) < 0) {
        D_DEBUG1("nl_connect(): ret=%d strerror='%s'\n", ret, strerror(errno));
        goto out;
    }

    err = rtnl_link_alloc_cache(cfg->sk, AF_UNSPEC, &link_cache);
    if (err < 0) {
        D_DEBUG1("rtnl_link_alloc_cache(): Unable to allocate cache err=%d\n", err);
        goto out;
    }

    if_index = rtnl_link_name2i(link_cache, gre_type);
    if (!if_index) {
        D_DEBUG1("rtnl_link_name2i(): Unable to lookup %s if_index=%d\n", gre_type, if_index);
        goto out;
    }

    link = rtnl_link_ipgretap_alloc();
    if (!link) {
        D_DEBUG1("rtnl_link_ipgre_alloc(): Unable to allocate link");
        goto out;
    }

    rtnl_link_set_name(link, gre_iface);
    rtnl_link_set_flags(link, IFF_UP);

    rtnl_link_ipgre_set_link(link, if_index);
    rtnl_link_ipgre_set_local(link, in_local->s_addr);
    rtnl_link_ipgre_set_remote(link, in_remote->s_addr);
    rtnl_link_ipgre_set_ttl(link, 64);
    rtnl_link_ipgre_set_iflags(link, 0);
    rtnl_link_ipgre_set_ikey(link, 0);
    rtnl_link_ipgre_set_oflags(link, 0);
    rtnl_link_ipgre_set_okey(link, 0);
    rtnl_link_ipgre_set_tos(link, 0);
    rtnl_link_ipgre_set_pmtudisc(link, 0);

    err = rtnl_link_add(cfg->sk, link, NLM_F_CREATE);
    if (err < 0) {
        D_DEBUG1("rtnl_link_add(): Unable to add link %s\n", gre_iface);
        goto out;
    }

    rtnl_link_put(link);
    ret = true;

out:
    nl_close(cfg->sk);

    return ret;
}

int
iface_gre_del(const char *gre_iface) {
    struct softgred_config *cfg = softgred_config_ref();
    struct rtnl_link *link;
    int ret = false;

    cfg->sk = nl_socket_alloc();
    if ((ret = nl_connect(cfg->sk, NETLINK_ROUTE)) < 0) {
        D_DEBUG1("rtnl_open ret=%d strerror='%s'\n", ret, strerror(errno));
        goto out;
    }

    link = rtnl_link_alloc();
    rtnl_link_set_name(link, gre_iface);

    if ((ret = rtnl_link_delete(cfg->sk, link)) < 0) {
        nl_perror(ret, "Unable to delete link");
        goto out;
    }

    rtnl_link_put(link);
    ret = true;

out:
    nl_close(cfg->sk);

    return ret;
}
