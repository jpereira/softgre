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

#ifndef _LOG_H__
#define _LOG_H__

#include <stdio.h>
#include "helper.h"

#ifdef DEBUG
#   define D(format, ...) fprintf(stderr, "%s:%d %s() - " format, __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#else
#   define D(format, ...) no_debug(0, format, ## __VA_ARGS__)
#endif

#ifdef DEVEL
#   define DD(format, ...) fprintf(stderr, " ** DEBUG: %s:%d %s() - " format, __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#   define DDF(format, ...) fprintf(stderr, format, ## __VA_ARGS__)
#else
#   define DD(format, ...) no_debug(0, format, )
#   define DDF(format, ...) no_debug(0, format, ## __VA_ARGS__)
#endif

#define TRACE(MESSAGE, ...) { \
  const char *A[] = { MESSAGE }; \
  printf("TRACE: %s %s %d\n",__FUNCTION__,__FILE__,__LINE__); fflush(stdout);\
  if(sizeof(A) > 0) \
	printf(*A, ## __VA_ARGS__); \
}

enum {
	L_CRIT,
	L_WARNING,
	L_NOTICE,
	L_INFO,
	L_DEBUG
};

#define D_CRIT(fmt, ...)    log_message(L_CRIT, fmt, ## __VA_ARGS__)
#define D_WARNING(fmt, ...) log_message(L_WARNING, fmt, ## __VA_ARGS__)
#define D_NOTICE(fmt, ...)  log_message(L_NOTICE, fmt, ## __VA_ARGS__)
#define D_INFO(fmt, ...)    log_message(L_INFO, fmt, ## __VA_ARGS__)
#define D_DEBUG(fmt, ...)   log_message(L_DEBUG, "**debug: %s:%d %s() "fmt, basename(__FILE__), __LINE__, __func__, ## __VA_ARGS__)

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
             const char *format,
             ...);

#endif
