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
#include "softgred_config.h"
#include "log.h"

static pthread_mutex_t lock;

static const int log_class[] = {
	[L_CRIT]    = LOG_CRIT,
	[L_WARNING] = LOG_WARNING,
	[L_NOTICE]  = LOG_NOTICE,
	[L_INFO]    = LOG_INFO,
	[L_DEBUG]   = LOG_DEBUG
};

void
log_init()
{
	openlog(PACKAGE, 0, LOG_DAEMON);
}

void
log_end()
{
	closelog();
}

void
log_message(int priority,
            const char *format, 
            ...)
{
    struct softgred_config *cfg = softgred_config_get();
	va_list vl;

    pthread_mutex_lock(&lock);
    if (priority == L_DEBUG && !cfg->debug_mode)
    {
        return;
    }

	va_start(vl, format);
    /* send to syslog */
//	vsyslog(log_class[priority], format, vl);

    /* send to output */
    if (cfg->debug_mode)
    {
        char *buf = NULL;
        vasprintf(&buf, format, vl);
        fprintf(stderr, "%s", buf);
        free(buf);
    }

	va_end(vl);
    pthread_mutex_unlock(&lock);
}

