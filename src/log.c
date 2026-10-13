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
static int log_fd = -1;
static bool write_failed;

int log_open(const char *path)
{
    if (!path || !g_path_is_absolute(path)) { errno = EINVAL; return -1; }
    int fd = open(path, O_WRONLY | O_APPEND | O_CREAT | O_CLOEXEC | O_NOFOLLOW | O_NONBLOCK,
                  0600);
    if (fd < 0) return -1;
    struct stat st;
    int error = 0;
    if (fstat(fd, &st)) error = errno;
    else if (!S_ISREG(st.st_mode) || st.st_nlink != 1 || st.st_uid != geteuid() ||
             (st.st_mode & S_IWOTH)) error = EPERM;
    if (error) { close(fd); errno = error; return -1; }
    if (log_fd >= 0) close(log_fd);
    log_fd = fd;
    write_failed = false;
    return 0;
}

void log_close(void)
{
    if (log_fd >= 0) close(log_fd);
    log_fd = -1;
    write_failed = false;
}

void log_message(int level, const char *format, ...)
{
    int saved_errno = errno;
    struct softgred_config *cfg = softgred_config_ref();
    if (level < 5 && level > cfg->dbg_mode) return;
    char stamp[40];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(stamp, sizeof(stamp), "%Y-%m-%dT%H:%M:%S%z", &tm);
    va_list args;
    va_start(args, format);
    char *message = g_strdup_vprintf(format, args);
    va_end(args);
    if (cfg->dbg_time) fprintf(stderr, "[%s] ", stamp);
    fputs(message, stderr);
    if (log_fd >= 0) {
        const char *severity = level < 5 ? "DEBUG" : level == 6 ? "WARNING" :
                               level == 7 ? "ERROR" : "INFO";
        char *line = g_strdup_printf("[%s] pid=%ld level=%s %s%s", stamp, (long)getpid(),
                                    severity, message, g_str_has_suffix(message, "\n") ? "" : "\n");
        size_t length = strlen(line), offset = 0;
        while (offset < length) {
            ssize_t written = write(log_fd, line + offset, length - offset);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) {
                if (!write_failed) fprintf(stderr, "Log write failed: %s\n",
                                           written < 0 ? strerror(errno) : "short write");
                write_failed = true;
                break;
            }
            offset += written;
        }
        if (offset == length) write_failed = false;
        g_free(line);
    }
    g_free(message);
    errno = saved_errno;
}
