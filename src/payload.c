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

#include "payload.h"

#include "log.h"
#include "provision.h"
#include "softgred_config.h"

static int capture_fd = -1;

static uint16_t read16(const unsigned char *p)
{
    return ((uint16_t)p[0] << 8) | p[1];
}

bool payload_parse(const unsigned char *data, size_t len, struct payload_packet *out)
{
    if (!data || !out || len < 14)
        return false;
    size_t offset = 14;
    uint16_t type = read16(data + 12);
    /* Ethernet capture may include one or two outer VLAN tags. */
    for (unsigned tags = 0; type == 0x8100 || type == 0x88a8; tags++) {
        if (tags == 2 || len - offset < 4)
            return false;
        type = read16(data + offset + 2);
        offset += 4;
    }
    if (type != 0x0800 || len - offset < 20)
        return false;
    const unsigned char *ip = data + offset;
    size_t ihl = (ip[0] & 15) * 4, total = read16(ip + 2);
    if ((ip[0] >> 4) != 4 || ihl < 20 || total < ihl + 4 || total > len - offset || ip[9] != 47 ||
        (read16(ip + 6) & 0xbfff))
        return false;
    /* Only unkeyed GRE v0 Ethernet bridging (TEB), matching our gretap endpoints. */
    const unsigned char *gre = ip + ihl;
    if (read16(gre) != 0 || read16(gre + 2) != 0x6558)
        return false;
    size_t remaining = total - ihl - 4;
    const unsigned char *ether = gre + 4;
    if (remaining < 14)
        return false;
    type = read16(ether + 12);
    if (type != 0x8100 || remaining < 18)
        return false;
    uint16_t inner_type = read16(ether + 16);
    if (inner_type == 0x8100 || inner_type == 0x88a8)
        return false;
    uint16_t vlan = read16(ether + 14) & 0x0fff;
    if (vlan < 1 || vlan > 4094 || (ether[6] & 1))
        return false;
    unsigned nonzero = 0;
    for (unsigned i = 0; i < 6; i++)
        nonzero |= ether[6 + i];
    if (!nonzero)
        return false;
    memcpy(&out->remote.s_addr, ip + 12, 4);
    memcpy(&out->local.s_addr, ip + 16, 4);
    memcpy(out->mac, ether + 6, 6);
    out->vlan = vlan;
    return true;
}

int payload_loop_init(void)
{
    struct softgred_config *cfg = softgred_config_ref();
    unsigned index = if_nametoindex(cfg->ifname);
    if (!index)
        return -1;
    capture_fd = socket(AF_PACKET, SOCK_RAW | SOCK_NONBLOCK | SOCK_CLOEXEC, htons(ETH_P_ALL));
    if (capture_fd < 0)
        return -1;
    struct sockaddr_ll addr = {
        .sll_family = AF_PACKET, .sll_protocol = htons(ETH_P_ALL), .sll_ifindex = (int)index};
    /* Kernel filter: IPv4 GRE, including up to two outer VLAN tags. */
    struct sock_filter code[] = {BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 12),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x0800, 0, 3),
                                 BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 23),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 47, 13, 14),
                                 BPF_STMT(BPF_RET | BPF_K, 0),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x8100, 1, 0),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x88a8, 0, 11),
                                 BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 16),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x0800, 0, 2),
                                 BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 27),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 47, 6, 7),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x8100, 1, 0),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x88a8, 0, 5),
                                 BPF_STMT(BPF_LD | BPF_H | BPF_ABS, 20),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0x0800, 0, 3),
                                 BPF_STMT(BPF_LD | BPF_B | BPF_ABS, 31),
                                 BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 47, 0, 1),
                                 BPF_STMT(BPF_RET | BPF_K, 65535),
                                 BPF_STMT(BPF_RET | BPF_K, 0)};
    struct sock_fprog filter = {.len = ARRAY_SIZE(code), .filter = code};
    int size = 4 * 1024 * 1024, ignore = 1;
    setsockopt(capture_fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
#ifdef PACKET_IGNORE_OUTGOING
    setsockopt(capture_fd, SOL_PACKET, PACKET_IGNORE_OUTGOING, &ignore, sizeof(ignore));
#else
    (void)ignore;
#endif
    if (bind(capture_fd, (struct sockaddr *)&addr, sizeof(addr)) ||
        setsockopt(capture_fd, SOL_SOCKET, SO_ATTACH_FILTER, &filter, sizeof(filter))) {
        payload_loop_end();
        return -1;
    }
    return capture_fd;
}

int payload_receive(void)
{
    unsigned char buffer[65536];
    /* Bounded batches keep the status API and signal handling responsive. */
    for (unsigned count = 0; count < 64; count++) {
        struct sockaddr_ll peer;
        socklen_t peer_len = sizeof(peer);
        ssize_t len =
            recvfrom(capture_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
        if (len < 0)
            return errno == EAGAIN || errno == EINTR ? 0 : -1;
        if (peer.sll_pkttype == PACKET_OUTGOING)
            continue;
        struct payload_packet packet;
        if (payload_parse(buffer, (size_t)len, &packet))
            provision_packet(&packet);
    }
    return 0;
}

void payload_loop_end(void)
{
    if (capture_fd >= 0)
        close(capture_fd);
    capture_fd = -1;
}
