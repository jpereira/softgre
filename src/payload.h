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

#ifndef PAYLOAD_H
#define PAYLOAD_H
#include "general.h"
struct payload_packet {
    struct in_addr remote, local;
    unsigned char mac[6];
    uint16_t vlan;
};
bool payload_parse(const unsigned char *data, size_t len, struct payload_packet *out);
int payload_loop_init(void);
int payload_receive(void);
void payload_loop_end(void);
#endif
