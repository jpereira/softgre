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

#include "iface_gre.h"
#include "log.h"
void link_request_init(struct link_request *req, int type, unsigned flags, unsigned index)
{
    memset(req, 0, sizeof(*req));
    req->header.nlmsg_len = NLMSG_LENGTH(sizeof(req->info));
    req->header.nlmsg_type = type;
    req->header.nlmsg_flags = NLM_F_REQUEST | NLM_F_ACK | flags;
    req->header.nlmsg_seq = 1;
    req->info.ifi_family = AF_UNSPEC;
    req->info.ifi_index = index;
}
int link_attr(struct link_request *req, unsigned type, const void *data, size_t len)
{
    size_t start = NLMSG_ALIGN(req->header.nlmsg_len), size = RTA_LENGTH(len);
    if (start + RTA_ALIGN(size) > sizeof(*req)) { errno = EMSGSIZE; return -1; }
    struct rtattr *attr = (struct rtattr *)((char *)req + start);
    attr->rta_type = type;
    attr->rta_len = size;
    if (len) memcpy(RTA_DATA(attr), data, len);
    req->header.nlmsg_len = start + RTA_ALIGN(size);
    return 0;
}
struct rtattr *link_nest(struct link_request *req, unsigned type)
{
    struct rtattr *attr = (struct rtattr *)((char *)req + NLMSG_ALIGN(req->header.nlmsg_len));
    return link_attr(req, type | NLA_F_NESTED, NULL, 0) ? NULL : attr;
}
void link_nest_end(struct link_request *req, struct rtattr *attr)
{
    attr->rta_len = (char *)req + req->header.nlmsg_len - (char *)attr;
}
int link_send(struct link_request *req)
{
    int fd = socket(AF_NETLINK, SOCK_RAW | SOCK_CLOEXEC, NETLINK_ROUTE);
    if (fd < 0) return -1;
    struct sockaddr_nl kernel = { .nl_family = AF_NETLINK };
    struct timeval timeout = { .tv_sec = 3 };
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    int ret = -1;
    if (sendto(fd, req, req->header.nlmsg_len, 0, (struct sockaddr *)&kernel,
               sizeof(kernel)) != (ssize_t)req->header.nlmsg_len) goto done;
    union { struct nlmsghdr alignment; unsigned char data[8192]; } reply;
    for (;;) {
        struct sockaddr_nl sender = {0};
        socklen_t sender_len = sizeof(sender);
        ssize_t got = recvfrom(fd, reply.data, sizeof(reply.data), 0,
                               (struct sockaddr *)&sender, &sender_len);
        if (got < 0) { if (errno == EINTR) continue; break; }
        if (sender.nl_pid != 0) continue;
        for (struct nlmsghdr *h = (struct nlmsghdr *)reply.data;
             NLMSG_OK(h, got); h = NLMSG_NEXT(h, got)) {
            if (h->nlmsg_seq != req->header.nlmsg_seq) continue;
            if (h->nlmsg_type == NLMSG_ERROR &&
                h->nlmsg_len >= NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
                struct nlmsgerr *error = NLMSG_DATA(h);
                if (!error->error) ret = 0;
                else errno = -error->error;
                goto done;
            }
        }
    }
done:
    { int saved = errno; close(fd); errno = saved; }
    return ret;
}
int iface_gre_add(const char *name, const char *kind, const char *dev,
                  const struct in_addr *local, const struct in_addr *remote)
{
    struct link_request req;
    uint32_t index = if_nametoindex(dev);
    uint8_t pmtu = 0;
    if (!index) { errno = ENODEV; return -1; }
    link_request_init(&req, RTM_NEWLINK, NLM_F_CREATE | NLM_F_EXCL, 0);
    req.info.ifi_flags = IFF_UP;
    req.info.ifi_change = IFF_UP;
    if (link_attr(&req, IFLA_IFNAME, name, strlen(name) + 1)) return -1;
    struct rtattr *info = link_nest(&req, IFLA_LINKINFO);
    if (!info || link_attr(&req, IFLA_INFO_KIND, kind, strlen(kind) + 1)) return -1;
    struct rtattr *data = link_nest(&req, IFLA_INFO_DATA);
    if (!data || link_attr(&req, IFLA_GRE_LINK, &index, sizeof(index)) ||
        link_attr(&req, IFLA_GRE_LOCAL, &local->s_addr, 4) ||
        link_attr(&req, IFLA_GRE_REMOTE, &remote->s_addr, 4) ||
        link_attr(&req, IFLA_GRE_PMTUDISC, &pmtu, 1)) return -1;
    link_nest_end(&req, data); link_nest_end(&req, info);
    return link_send(&req);
}
int iface_gre_del(const char *name)
{
    unsigned index = if_nametoindex(name);
    if (!index) { errno = ENODEV; return -1; }
    struct link_request req;
    link_request_init(&req, RTM_DELLINK, 0, index);
    return link_send(&req);
}
