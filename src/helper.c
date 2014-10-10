/**
 *  This file is part of SoftGREd
 *
 *    SoftGREd is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Foundation, either
 *  version 3 of the License, or (at your option) any later version.
 *
 *  SoftGREd is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with SoftGREd.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Copyright (C) 2014, Jorge Pereira <jpereiran@gmail.com>
 */

#define _GNU_SOURCE

#include "helper.h"
#include "log.h"
#include "general.h"

/*
 * print data in rows of 16 bytes: offset   hex   ascii
 */
void
helper_print_hex2ascii(const u_char *payload,
					   size_t len,
					   int offset)
{
    size_t i;
    size_t gap;
    const u_char *ch;

    /* offset */
    printf("%05d   ", offset);
    
    /* hex */
    ch = payload;
    for(i = 0; i < len; i++)
	{
        printf("%02x ", *ch);
        ch++;
        /* print extra space after 8th byte for visual aid */
        if (i == 7)
            printf(" ");
    }
    /* print space to handle line less than 8 bytes */
    if (len < 8)
        printf(" ");
    
    /* fill hex gap with spaces if not full line */
    if (len < 16)
	{
        gap = 16 - len;
        for (i = 0; i < gap; i++) {
            printf("   ");
        }
    }
    printf("   ");
    
    /* ascii (if printable) */
    ch = payload;
    for(i = 0; i < len; i++)
	{
        if (isprint(*ch))
            printf("%c", *ch);
        else
            printf(".");
        ch++;
    }

    printf("\n");
}

/*
 * print packet payload data (avoid printing binary data)
 */
void
helper_print_payload (const u_char *payload,
                      size_t len)
{
    size_t len_rem = len;
    size_t line_width = 16;            /* number of bytes per line */
    size_t line_len;
    size_t offset = 0;                 /* zero-based offset counter */
    const u_char *ch = payload;

    if (len <= 0)
        return;

    D_DEBUG3("<print log payload=%#08x len(%ld)>\n", payload, len);

    /* data fits on one line */
    if (len <= line_width)
	{
        helper_print_hex2ascii(ch, len, offset);
        D_DEBUG3("</payload log>\n");
        return;
    }

    /* data spans multiple lines */
    for (;;)
	{
        /* compute current line length */
        line_len = line_width % len_rem;
        /* print line */
        helper_print_hex2ascii(ch, line_len, offset);
        /* compute total remaining */
        len_rem = len_rem - line_len;
        /* shift pointer to remaining bytes to print */
        ch = ch + line_len;
        /* add offset */
        offset = offset + line_width;
        /* check if we have line width chars or less */
        if (len_rem <= line_width)
		{
            /* print last line and get out */
            helper_print_hex2ascii(ch, len_rem, offset);
            break;
        }
    }

    D_DEBUG3("</payload log>\n");
}

int
helper_system(bool verbose,
              const char *format, 
              ...)
{
    va_list vl;
    char *buf = NULL;
    char *cmd = NULL;
    int ret;

    va_start(vl, format);
    vasprintf(&buf, format, vl);

    if_debug(cmd, D_DEBUG3("call system() with '%s'\n", buf));

    asprintf(&cmd, "%s %s", buf, !verbose ? "1> /dev/null 2>&1" : "");

    if ((ret = system(cmd)) != 0)
    {
        if_debug(cmd, D_CRIT("Problems with system() runs '%s'\n", buf));
    }

    free(cmd);
    free(buf);
    va_end(vl);

    return ret;
}

void
helper_enable_high_priority()
{
    int which = PRIO_PROCESS;
    id_t pid = getpid();
    int ret = getpriority(which, pid);
    int priority = -20;

    D_DEBUG0("Current pid %d with priority is %d, change to %d\n", pid, ret, priority);

    if (setpriority(which, pid, priority) < 0)
    {
        D_CRIT("Problems with setpriority(), errno=%d (%s)\n", errno, strerror(errno));
    }

}

void
helper_enable_coredump()
{
    struct rlimit core_limit = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY
    };

    setrlimit(RLIMIT_CORE, &core_limit);
}

void
helper_print_stacktrace()
{
    void *array[10];
    size_t size;
    char **strings;
    size_t i;

    D_DEBUG0("<begin>\n");

    size = backtrace (array, 10);
    strings = backtrace_symbols ((void *const *)array, size);

    for (i = 0; i < size; i++)
    {
        fprintf(stderr, "  [%02ld] %s\n", i, strings[i]);
    }

    free (strings);
    D_DEBUG0("<end>\n");
}

