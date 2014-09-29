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
#define _GNU_SOURCE

#include "general.h"
#include "iface_gre.h"
#include "log.h"

struct iplink_req {                                                                    
    struct nlmsghdr n;
    struct ifinfomsg i;
    char buf[1024];
};

static int have_rtnl_newlink = -1;
struct rtnl_handle rth = { .fd = -1 };

int
accept_msg(const struct sockaddr_nl* UNUSED(who),
           struct nlmsghdr* n,
           void* UNUSED(arg))
{
    struct nlmsgerr *err = (struct nlmsgerr *)NLMSG_DATA(n);

    if (n->nlmsg_type == NLMSG_ERROR &&
            (err->error == -EOPNOTSUPP || err->error == -EINVAL))
        have_rtnl_newlink = 0; 
    else 
        have_rtnl_newlink = 1; 
    return -1;
}

static int
iplink_have_newlink(void)                                                        
{
    struct iplink_req req;

    if (have_rtnl_newlink < 0)
    {
        memset(&req, 0, sizeof(req));

        req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
        req.n.nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
        req.n.nlmsg_type = RTM_NEWLINK;
        req.i.ifi_family = AF_UNSPEC;

        if (rtnl_send(&rth, &req.n, req.n.nlmsg_len) < 0)
        {
            perror("request send failed");
            exit(1);
        }
        rtnl_listen(&rth, accept_msg, NULL);
    }
    return have_rtnl_newlink;
}

int
iplink_modify(int cmd,
              int flags,
              const char *gre_iface,
              const char *dev_to_link,
              const char *type,
              const struct in_addr *in_local,
              const struct in_addr *in_remote)
{
    int ret;
    struct iplink_req req;
    uint16_t iflags = 0;
    uint16_t oflags = 0;
    unsigned ikey = 0;
    unsigned okey = 0;
    unsigned link = 0;
    uint8_t pmtudisc = 0; // pmtudisc=1, nopmtudisc=0
    uint8_t ttl = 0; // 255?
    uint8_t tos = 0;
    int preferred_family = AF_PACKET;

    memset(&req, 0, sizeof(req));

    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.n.nlmsg_flags =  NLM_F_REQUEST | flags;
    req.n.nlmsg_type = cmd;
    req.i.ifi_family = preferred_family;

    size_t len = strlen(gre_iface) + 1;
    if (len == 1)
    {
        D_DEBUG("%s is not a valid device identifier\n");
    }
    if (len > IFNAMSIZ)
    {
        D_DEBUG("new gre iface '%s' is too long\n", gre_iface);
    }
    addattr_l(&req.n, sizeof(req), IFLA_IFNAME, gre_iface, len+1);

    struct rtattr *linkinfo = NLMSG_TAIL(&req.n);
    addattr_l(&req.n, sizeof(req), IFLA_LINKINFO, NULL, 0);

    if (type)
        addattr_l(&req.n, sizeof(req), IFLA_INFO_KIND, type, strlen(type));

    struct rtattr * data = NLMSG_TAIL(&req.n);
    addattr_l(&req.n, sizeof(req), IFLA_INFO_DATA, NULL, 0);

    if (cmd == RTM_NEWLINK)
    {
        req.i.ifi_change |= IFF_UP;
        req.i.ifi_flags |= IFF_UP;

        link = if_nametoindex(dev_to_link);
        if (link < 1)
        {
            D_DEBUG("Impossible to link a new gre-interface with %s interface!\n", dev_to_link);
            return -1;
        }

        addattr32(&req.n, 1024, IFLA_GRE_IKEY, ikey);
        addattr32(&req.n, 1024, IFLA_GRE_OKEY, okey);
        addattr_l(&req.n, 1024, IFLA_GRE_IFLAGS, &iflags, 2);
        addattr_l(&req.n, 1024, IFLA_GRE_OFLAGS, &oflags, 2);
        addattr_l(&req.n, 1024, IFLA_GRE_LOCAL, &in_local->s_addr, 4);
        addattr_l(&req.n, 1024, IFLA_GRE_REMOTE, &in_remote->s_addr, 4);
        addattr_l(&req.n, 1024, IFLA_GRE_PMTUDISC, &pmtudisc, 1);

	    addattr_l(&req.n, 1024, IFLA_GRE_TTL, &ttl, 1);
	    addattr_l(&req.n, 1024, IFLA_GRE_TOS, &tos, 1);

        if (link)
            addattr32(&req.n, 1024, IFLA_GRE_LINK, link);

    } else {
        link = if_nametoindex(gre_iface);
        if (link < 1)
        {
            D_DEBUG("Impossible to link a new gre-interface with %s interface!\n", dev_to_link);
            return -1;
        }
    }

    data->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)data;
    linkinfo->rta_len = (void *)NLMSG_TAIL(&req.n) - (void *)linkinfo;

    // sending the packet to the kernel.
    if ((ret = rtnl_talk(&rth, &req.n, 0, 0, NULL)) < 0)
    {
        D_DEBUG("rtnl_talk() ret=%d strerror='%s'\n", ret, strerror(errno));
    }

    return ret;
}

int
iface_gre_add(const char *gre_iface,
              const char *type,
              const char *dev_to_link,
              const struct in_addr *in_local,
              const struct in_addr *in_remote)
{
    int ret = true;
    char *ip_local = strdupa(inet_ntoa(*in_local));
    char *ip_remote = strdupa(inet_ntoa(*in_remote));

    D_DEBUG("Creating gre-interface '%s' { .local='%s', .remote='%s' }\n", 
            gre_iface, ip_local, ip_remote
    );

    if ((ret=rtnl_open(&rth, 0)) < 0)
    {
        D_DEBUG("rtnl_open ret=%d strerror='%s'\n", ret, strerror(errno));
        return EXIT_FAILURE;
    }

    if (!iplink_have_newlink())
    {
        D_DEBUG("iplink_have_newlink ret=%d strerror='%s'\n", ret, strerror(errno));
    }
    else
    {
        ret = iplink_modify(RTM_NEWLINK,
                            NLM_F_CREATE | NLM_F_EXCL,
                            gre_iface,
                            dev_to_link,
                            type,
                            in_local,
                            in_remote);
        if (!ret)
            D_DEBUG("iplink_modify ret=%d strerror='%s'\n", ret, strerror(errno));
    }

    rtnl_close(&rth);

    return 1;
}

int
iface_gre_del(const char *gre_iface)
{
    int ret;
    errno = 0;

    if ((ret=rtnl_open(&rth, 0)) < 0)
    {
        D_DEBUG("Cannot open rtnetlink\n");
        return EXIT_FAILURE;
    }

    if(!iplink_have_newlink())
    {
        D_DEBUG("iplink_have_newlink ret=%d strerror='%s'\n", ret, strerror(errno));
    }
    else
    {
        ret = iplink_modify(RTM_DELLINK, 0, gre_iface, NULL, NULL, NULL, NULL);
        if (!ret)
        {
            D_DEBUG("iplink_modify ret=%d strerror='%s'\n", ret, strerror(errno));
        }
    }

    rtnl_close(&rth);

    return 1;
}
