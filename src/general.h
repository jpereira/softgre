/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */

#ifndef GENERAL_H_
#define GENERAL_H_

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

// Project config
#include "config.h"

// standards
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include <errno.h>
#include <assert.h>

#include <locale.h>
#include <signal.h>
#include <getopt.h>

// sys*
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>

// pthreads
#include <pthread.h>

// Network && Interfaces
#include <net/if.h>
#include <netinet/ether.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <libnetlink.h>

#include <linux/rtnetlink.h>
#include <linux/if_tunnel.h>

// libdhash
#include <dhash.h>

// pcap
#ifndef HAVE_PCAP_PCAP_H
    #error "Oops! Impossible to continue without pcap.h, exiting..."
#endif
#include <pcap.h>

// syslog
#include <syslog.h>

// get/setifaddrs
#include <ifaddrs.h>

#include "version.h" /* Generated automatic */

#ifndef ARRAY_SIZE
#   define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#   define ARRAY_AND_SIZE(x) (x), ARRAY_SIZE(x)
#endif

#ifdef __GNUC__
#  define UNUSED(x) UNUSED_ ## x __attribute__((__unused__))
#else
#  define UNUSED(x) UNUSED_ ## x
#endif

#ifdef __GNUC__
#  define UNUSED_FUNCTION(x) __attribute__((__unused__)) UNUSED_ ## x
#else
#  define UNUSED_FUNCTION(x) UNUSED_ ## x
#endif

#ifndef FREE
#   define FREE(x) do { free(x); x = NULL; } while (0);
#endif 

#define print_bool(x)   ((x == true) ? "true" : "false")

// commands
#define SOFTGRED_CMD_BRCTL  "/sbin/brctl"
#define SOFTGRED_CMD_IP     "/bin/ip"

#endif /*GENERAL_H_*/

