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

#ifndef GENERAL_H_
#define GENERAL_H_

#ifndef _GNU_SOURCE
#   define _GNU_SOURCE
#endif

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

// Network
#include <netinet/in.h>
#include <arpa/inet.h>

// pcap
#include <pcap.h>

// syslog
#include <syslog.h>

// get/setifaddrs
#include <ifaddrs.h>

// Project config
#include "config.h"

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

#endif /*GENERAL_H_*/

