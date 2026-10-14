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

int helper_exec(const char *const argv[])
{
    pid_t child = fork();
    if (child < 0) {
        D_WARNING("%s: fork: %s\n", argv[0], strerror(errno));
        return -1;
    }
    if (child == 0) {
        /* Child: restore the default signal mask, silence stdout, then exec on PATH. The
         * daemon's descriptors are O_CLOEXEC, so they do not leak into the command. */
        sigset_t empty;
        sigemptyset(&empty);
        sigprocmask(SIG_SETMASK, &empty, NULL);
        int devnull = open("/dev/null", O_WRONLY | O_CLOEXEC);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            if (devnull != STDOUT_FILENO)
                close(devnull);
        }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    int64_t deadline = monotonic_usec() + 3 * USEC_PER_SEC;
    int status = 0;
    for (;;) {
        pid_t waited = waitpid(child, &status, WNOHANG);
        if (waited == child)
            break;
        if (waited < 0 && errno != EINTR) {
            D_WARNING("%s: waitpid: %s\n", argv[0], strerror(errno));
            return -1;
        }
        if (monotonic_usec() >= deadline) {
            kill(child, SIGKILL);
            while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
            }
            D_WARNING("%s: command exceeded three-second deadline\n", argv[0]);
            return -1;
        }
        sleep_usec(1000);
    }
    if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        return 0;
    if (WIFSIGNALED(status))
        D_WARNING("%s: killed by signal %d\n", argv[0], WTERMSIG(status));
    else if (WIFEXITED(status))
        D_WARNING("%s: exited with status %d\n", argv[0], WEXITSTATUS(status));
    else
        D_WARNING("%s: execution failed\n", argv[0]);
    return -1;
}

void helper_print_payload(const unsigned char *data, size_t len)
{
    for (size_t offset = 0; offset < len; offset += 16) {
        printf("%05zu  ", offset);
        for (size_t i = offset; i < MIN(offset + 16, len); i++)
            printf("%02x ", data[i]);
        putchar('\n');
    }
}
