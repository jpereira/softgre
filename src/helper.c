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

#include "helper.h"
#include "log.h"
static void child_signals(void *unused)
{
    (void)unused;
    sigset_t empty;
    sigemptyset(&empty);
    sigprocmask(SIG_SETMASK, &empty, NULL);
}
int helper_exec(const char *const argv[])
{
    GError *error = NULL;
    int status = 0;
    GPid child;
    bool ok = g_spawn_async(NULL, (char **)argv, NULL,
                           G_SPAWN_SEARCH_PATH | G_SPAWN_STDOUT_TO_DEV_NULL |
                           G_SPAWN_DO_NOT_REAP_CHILD,
                           child_signals, NULL, &child, &error);
    if (ok) {
        gint64 deadline = g_get_monotonic_time() + 3 * G_USEC_PER_SEC;
        for (;;) {
            pid_t waited = waitpid(child, &status, WNOHANG);
            if (waited == child) break;
            if (waited < 0 && errno != EINTR) { ok = false; break; }
            if (g_get_monotonic_time() >= deadline) {
                kill(child, SIGKILL);
                while (waitpid(child, &status, 0) < 0 && errno == EINTR) {}
                g_set_error_literal(&error, G_SPAWN_ERROR, G_SPAWN_ERROR_FAILED,
                                    "command exceeded three-second deadline");
                ok = false; break;
            }
            g_usleep(1000);
        }
        g_spawn_close_pid(child);
        if (ok) ok = g_spawn_check_wait_status(status, &error);
    }
    if (!ok) {
        D_WARNING("%s: %s\n", argv[0], error ? error->message : "execution failed");
        g_clear_error(&error);
    }
    return ok ? 0 : -1;
}
void helper_print_payload(const unsigned char *data, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        printf("%05zu  ", offset);
        for (size_t i = offset; i < MIN(offset + 16, len); i++) printf("%02x ", data[i]);
        putchar('\n');
    }
}
