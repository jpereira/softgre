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
    [L_DEBUG4]  = { "dbg-insane", LOG_DEBUG   },
    [L_CRIT]    = { "critical",   LOG_CRIT    },
    [L_WARNING] = { "warning",    LOG_WARNING },
    [L_NOTICE]  = { "notice",     LOG_NOTICE  },
    [L_INFO]    = { "info",       LOG_INFO    }
};

void
log_init() {

}

void
log_end() {

}

void
log_message(int priority,
            const char *funcname,
            const char *filename,
            int lineno,
            const char *format, 
            ...) {
    va_list vl;
    struct softgred_config *cfg = softgred_config_ref();
    const char *label = &log_class[priority].label[0];
    const char *file = basename(filename);
    char *buf = NULL;
    int ret;

    va_start(vl, format);
    ret = vasprintf(&buf, format, vl);
    if (ret < 0) {
        fprintf(stderr, "Problems with vasprintf()\n");
    }

    if (cfg->dbg_time) {
        time_t t = time(NULL);
        struct tm tm = *localtime(&t);

        fprintf(stderr, "[%04d-%02d-%02d %02d:%02d:%02d] ", tm.tm_year + 1900, tm.tm_mon + 1,
                    tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
    }

    switch(cfg->dbg_mode) {
        case L_DEBUG2:
            fprintf(stderr, "** %s: %s", label, buf);
            break;
        case L_DEBUG3:
        case L_DEBUG4:
            fprintf(stderr, "** %s %s:%d %s(): %s", label, file, lineno, funcname, buf);
            break;
        default:
            fprintf(stderr, "%s", buf);
    }

    free(buf);
    va_end(vl);
}
