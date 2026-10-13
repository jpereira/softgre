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

#include "log.h"
#include "softgred_config.h"
void log_message(int level, const char *format, ...)
{
    struct softgred_config *cfg = softgred_config_ref();
    if (level < 5 && level > cfg->dbg_mode) return;
    if (cfg->dbg_time) {
        char stamp[32];
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        strftime(stamp, sizeof(stamp), "%Y-%m-%d %H:%M:%S", &tm);
        fprintf(stderr, "[%s] ", stamp);
    }
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
