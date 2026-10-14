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

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "compat.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static void *check(void *p)
{
    if (!p) {
        fputs("softgred: out of memory\n", stderr);
        abort();
    }
    return p;
}

void *xmalloc(size_t size)
{
    return check(malloc(size ? size : 1));
}

void *xcalloc(size_t count, size_t size)
{
    return check(calloc(count ? count : 1, size ? size : 1));
}

void *xrealloc(void *ptr, size_t size)
{
    return check(realloc(ptr, size ? size : 1));
}

char *xstrdup(const char *s)
{
    return s ? check(strdup(s)) : NULL;
}

char *xstrdup_vprintf(const char *format, va_list ap)
{
    va_list copy;
    va_copy(copy, ap);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed < 0)
        return check(NULL);
    char *out = xmalloc((size_t)needed + 1);
    vsnprintf(out, (size_t)needed + 1, format, ap);
    return out;
}

char *xstrdup_printf(const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    char *out = xstrdup_vprintf(format, ap);
    va_end(ap);
    return out;
}

char *str_trim(char *s)
{
    if (!s)
        return s;
    char *start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

char *str_split_next(char **cursor, char delim)
{
    char *field = *cursor;
    if (!field)
        return NULL;
    char *found = strchr(field, delim);
    if (found) {
        *found = '\0';
        *cursor = found + 1;
    } else {
        *cursor = NULL;
    }
    return field;
}

bool str_has_suffix(const char *s, const char *suffix)
{
    if (!s || !suffix)
        return false;
    size_t sl = strlen(s), xl = strlen(suffix);
    return xl <= sl && memcmp(s + sl - xl, suffix, xl) == 0;
}

bool path_is_absolute(const char *path)
{
    return path && path[0] == '/';
}

size_t str_lcpy(char *dst, const char *src, size_t size)
{
    size_t len = strlen(src);
    if (size) {
        size_t copy = len < size - 1 ? len : size - 1;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

bool find_in_path(const char *name, char *out, size_t size)
{
    const char *path = getenv("PATH");
    if (!path || !*path)
        path = "/usr/sbin:/usr/bin:/sbin:/bin";
    for (const char *p = path; *p;) {
        const char *colon = strchr(p, ':');
        size_t dir = colon ? (size_t)(colon - p) : strlen(p);
        int n = snprintf(out, size, "%.*s/%s", (int)dir, p, name);
        if (n > 0 && (size_t)n < size && access(out, X_OK) == 0)
            return true;
        p += dir + (colon ? 1 : 0);
    }
    return false;
}

int64_t monotonic_usec(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * USEC_PER_SEC + ts.tv_nsec / 1000;
}

void sleep_usec(int64_t usec)
{
    if (usec <= 0)
        return;
    struct timespec ts = {.tv_sec = usec / USEC_PER_SEC, .tv_nsec = (usec % USEC_PER_SEC) * 1000};
    nanosleep(&ts, NULL);
}

struct strbuf *strbuf_new(const char *init)
{
    struct strbuf *b = xcalloc(1, sizeof(*b));
    b->cap = 64;
    b->str = xmalloc(b->cap);
    b->str[0] = '\0';
    if (init)
        strbuf_append(b, init);
    return b;
}

void strbuf_free(struct strbuf *b)
{
    if (!b)
        return;
    free(b->str);
    free(b);
}

static void strbuf_reserve(struct strbuf *b, size_t extra)
{
    if (b->len + extra + 1 <= b->cap)
        return;
    size_t want = b->cap;
    while (want < b->len + extra + 1)
        want *= 2;
    b->str = xrealloc(b->str, want);
    b->cap = want;
}

void strbuf_append_len(struct strbuf *b, const char *s, size_t n)
{
    strbuf_reserve(b, n);
    memcpy(b->str + b->len, s, n);
    b->len += n;
    b->str[b->len] = '\0';
}

void strbuf_append(struct strbuf *b, const char *s)
{
    strbuf_append_len(b, s, strlen(s));
}

void strbuf_append_char(struct strbuf *b, char c)
{
    strbuf_append_len(b, &c, 1);
}

void strbuf_appendf(struct strbuf *b, const char *format, ...)
{
    va_list ap;
    va_start(ap, format);
    va_list copy;
    va_copy(copy, ap);
    int needed = vsnprintf(NULL, 0, format, copy);
    va_end(copy);
    if (needed > 0) {
        strbuf_reserve(b, (size_t)needed);
        vsnprintf(b->str + b->len, (size_t)needed + 1, format, ap);
        b->len += (size_t)needed;
    }
    va_end(ap);
}

void strbuf_assign(struct strbuf *b, const char *s)
{
    b->len = 0;
    b->str[0] = '\0';
    strbuf_append(b, s);
}
