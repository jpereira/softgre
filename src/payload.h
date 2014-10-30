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

#ifndef PAYLOAD_H_
#define PAYLOAD_H_

#include "general.h"

/* default snap length (maximum bytes per packet to capture) */
#define SNAP_LEN            1518

/**
 * from rfc2784
 * The GRE packet header has the form:
 *
 *  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |C|       Reserved0       | Ver |         Protocol Type         |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 * |      Checksum (optional)      |       Reserved1 (Optional)    |
 * +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
#define GRE_LENGHT          4 /* bytes */

struct payload_gre {
    unsigned has_checksum :  1; // Checksum Present (bit 0)
    unsigned reserved0    : 12; // Reserved0 (bits 1-12)
    unsigned ver          :  3; // Version Number (bits 13-15)
    unsigned type         : 16; // Protocol Type (2 octets)
    unsigned checksum     : 16; // Checksum (2 octets)
    unsigned reserved1    : 16; // Reserved1 (2 octets)
};

struct payload_config {
    struct bpf_program fp;         /* compiled filter program (expression) */
    pcap_t *handle;                /* packet capture handle */
    char errbuf[PCAP_ERRBUF_SIZE]; /* error buffer */
    bpf_u_int32 mask;              /* subnet mask */
    bpf_u_int32 net;               /* ip */
};

struct payload_config *payload_config_ref();

int payload_loop_init();

int payload_loop_run();

void payload_loop_end();

void payload_handler_packet_cb(u_char *args,
                               const struct pcap_pkthdr *header,
                               const u_char *packet);

#endif /*PAYLOAD_H_*/

