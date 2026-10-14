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

#ifndef GENERAL_H
#define GENERAL_H
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "config.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <linux/filter.h>
#include <linux/if_packet.h>
#include <linux/if_tunnel.h>
#include <linux/rtnetlink.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define SOFTGRED_MAX_ATTACH 10
#define SOFTGRED_MAX_SLOTS 4096
#define PROVISION_MAX_CLIENTS 1024
#define SOFTGRED_MAX_ALLOW 64
#define SOFTGRED_DEFAULT_LOGFILE "/var/log/softgred.log"
#define SOFTGRED_DEFAULT_CONFFILE SOFTGRED_CONFDIR "/softgred.conf"
#define SOFTGRED_DEFAULT_PIDFILE SOFTGRED_PIDDIR "/softgred.pid"
#endif
