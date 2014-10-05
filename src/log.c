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

struct log_sets {
    const char *label;
    int level;
};

static struct log_sets
log_class[] = {
    [L_DEBUG0]  = { "msg",        LOG_DEBUG   },    
    [L_DEBUG1]  = { "dbg",        LOG_DEBUG   },
    [L_DEBUG2]  = { "dbg-devel",  LOG_DEBUG   },
    [L_DEBUG3]  = { "dbg-crazy",  LOG_DEBUG   },
    [L_CRIT]    = { "critical",   LOG_CRIT    },
    [L_WARNING] = { "warning",    LOG_WARNING },
    [L_NOTICE]  = { "notice",     LOG_NOTICE  },
    [L_INFO]    = { "info",       LOG_INFO    }
};

void
log_init()
{

}

void
log_end()
{

}

void
log_message(int priority,
            const char *funcname,
            const char *filename,
            int lineno,
            const char *format, 
            ...)
{
    va_list vl;
    struct softgred_config *cfg = softgred_config_get();
    const char *label = &log_class[priority].label[0];
    const char *file = basename(filename);
    char *buf = NULL;

    va_start(vl, format);
    vasprintf(&buf, format, vl);

    switch(cfg->debug_mode)
    {
        case L_DEBUG2:
            fprintf(stderr, "** %s: %s", label, buf);
            break;
        case L_DEBUG3:
            fprintf(stderr, "** %s %s:%d %s(): %s", label, file, lineno, funcname, buf);
            break;
        default:
            fprintf(stderr, "%s", buf);
    }

    free(buf);

    va_end(vl);
}

