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

#ifndef _LOG_H__
#define _LOG_H__

#include <stdio.h>
#include "helper.h"

#define DEBUG_MAX_LEVEL     4

#ifdef DEBUG
#   define D(format, ...) fprintf(stderr, "%s:%d %s() - " format, __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#else
#   define D(format, ...) no_debug(0, 0, format, ## __VA_ARGS__)
#endif

#ifdef DEVEL
#   define DD(format, ...) fprintf(stderr, " ** DEBUG: %s:%d %s() - " format, __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#   define DDF(format, ...) fprintf(stderr, format, ## __VA_ARGS__)
#else
#   define DD(format, ...) no_debug(0, 0, format, )
#   define DDF(format, ...) no_debug(0, 0, format, ## __VA_ARGS__)
#endif

#define TRACE(MESSAGE, ...) { \
  const char *A[] = { MESSAGE }; \
  printf("TRACE: %s %s %d\n",__FUNCTION__,__FILE__,__LINE__); fflush(stdout);\
  if(sizeof(A) > 0) \
    printf(*A, ## __VA_ARGS__); \
}

enum {
    L_DEBUG0,
    L_DEBUG1,
    L_DEBUG2,
    L_DEBUG3,
    L_DEBUG4,
    L_CRIT,
    L_WARNING,
    L_NOTICE,
    L_INFO
};

#define D_DEBUG0(fmt, ...)  log_message(L_DEBUG0,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_DEBUG1(fmt, ...)  log_message(L_DEBUG1,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_DEBUG2(fmt, ...)  log_message(L_DEBUG2,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_DEBUG3(fmt, ...)  log_message(L_DEBUG3,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_DEBUG4(fmt, ...)  log_message(L_DEBUG4,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_CRIT(fmt, ...)    log_message(L_CRIT,    __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_WARNING(fmt, ...) log_message(L_WARNING, __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_NOTICE(fmt, ...)  log_message(L_NOTICE,  __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)
#define D_INFO(fmt, ...)    log_message(L_INFO,    __func__, __FILE__, __LINE__, fmt, ## __VA_ARGS__)

static inline void
no_debug (int UNUSED(level),
          const char* UNUSED(fmt),
          ...)
{

    /* none */
}

void
log_init();

void
log_end();

void
log_message (int priority,
             const char *funcname,
             const char *filename,
             int lineno,
             const char *format,
             ...);

#endif

