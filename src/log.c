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

//static pthread_mutex_t log_lock;

struct log_sets {
    const char *label;
    int level;
};

static struct log_sets
log_class[] = {
    [L_CRIT]    = { "critical", LOG_CRIT },
    [L_WARNING] = { "warning",  LOG_WARNING },
    [L_NOTICE]  = { "notice",   LOG_NOTICE },
    [L_INFO]    = { "info",     LOG_INFO },
    [L_DEBUG]   = { "debug",    LOG_DEBUG },
    [L_DEBUG2]  = { "debug-crazy",    LOG_DEBUG }
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
            const char *funcname,
            const char *filename,
            int lineno,
            const char *format, 
            ...)
{
    struct softgred_config *cfg;
    va_list vl;

//    pthread_mutex_lock(&log_lock);
    cfg = softgred_config_get();

    if (priority == L_DEBUG && cfg->debug_mode == 0)
    {
        return;
    }

    va_start(vl, format);
    /* send to syslog */
//    vsyslog(log_class[priority], format, vl);

    /* send to output */
    if (cfg->debug_mode > 0)
    {
        const char *label = &log_class[priority].label[0];
        const char *file = basename(filename);
        char *buf = NULL;

        vasprintf(&buf, format, vl);

        switch(cfg->debug_mode)
        {
            case 1:
                fprintf(stderr, "%s", buf);
                break;
            case 2:
                fprintf(stderr, "** %s: %s", label, buf);
                break;
            default:
                fprintf(stderr, "** %s %s:%d %s(): %s", label, file, lineno, funcname, buf);
        }

        free(buf);
    }

    va_end(vl);
    //pthread_mutex_unlock(&log_lock);
}

