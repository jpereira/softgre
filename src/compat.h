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

#ifndef COMPAT_H
#define COMPAT_H
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Small libc-only replacements for the GLib utility functions SoftGRE used. Allocation
 * failures abort, matching GLib's g_malloc contract and keeping call sites simple. */
void *xmalloc(size_t size);
void *xcalloc(size_t count, size_t size);
void *xrealloc(void *ptr, size_t size);
char *xstrdup(const char *s); /* NULL-safe: xstrdup(NULL) returns NULL. */
char *xstrdup_printf(const char *format, ...) __attribute__((format(printf, 1, 2)));
char *xstrdup_vprintf(const char *format, va_list ap) __attribute__((format(printf, 1, 0)));
char *str_trim(char *s); /* Strip leading/trailing ASCII whitespace in place; returns s. */
/* Split on delim in place: returns the next field (empties preserved), advances *cursor
 * past the delimiter, and yields NULL when exhausted. */
char *str_split_next(char **cursor, char delim);
bool str_has_suffix(const char *s, const char *suffix);
bool path_is_absolute(const char *path);
size_t str_lcpy(char *dst, const char *src, size_t size); /* strlcpy semantics. */
/* Locate an executable by name on $PATH; writes the full path to out and returns true. */
bool find_in_path(const char *name, char *out, size_t size);
int64_t monotonic_usec(void);
void sleep_usec(int64_t usec);

#define USEC_PER_SEC INT64_C(1000000)
#define ascii_isdigit(c) ((c) >= '0' && (c) <= '9')
#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static inline bool ascii_isalnum(unsigned char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

static inline bool ascii_isxdigit(unsigned char c)
{
    return ascii_isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static inline char ascii_tolower(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') ? (char)(c + 32) : (char)c;
}

/* Growable string buffer replacing GLib's GString for the status API responses. */
struct strbuf {
    char *str;
    size_t len, cap;
};

struct strbuf *strbuf_new(const char *init); /* init may be NULL. */
void strbuf_free(struct strbuf *b);          /* Frees the buffer and its storage. */
void strbuf_append(struct strbuf *b, const char *s);
void strbuf_append_len(struct strbuf *b, const char *s, size_t n);
void strbuf_append_char(struct strbuf *b, char c);
void strbuf_appendf(struct strbuf *b, const char *format, ...)
    __attribute__((format(printf, 2, 3)));
void strbuf_assign(struct strbuf *b, const char *s); /* Replace contents with s. */
#endif
