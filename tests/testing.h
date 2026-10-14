/* Minimal test harness replacing the GLib test framework: a registry, a runner and the
 * g_test_ and g_assert_ macros the suites already use, plus a few file/regex/random helpers. */
#ifndef TESTING_H
#define TESTING_H
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef void (*test_func)(void);
void testing_add(const char *name, test_func fn);
int testing_run(void);
extern const char *testing_current;

#define g_test_init(argc, argv, unused) ((void)(argc), (void)(argv), (void)(unused))
#define g_test_add_func(name, fn) testing_add((name), (fn))
#define g_test_run() testing_run()

#define TESTING_FAIL(...)                                                                          \
    do {                                                                                           \
        fprintf(stderr, "FAIL [%s] %s:%d: ", testing_current, __FILE__, __LINE__);                 \
        fprintf(stderr, __VA_ARGS__);                                                              \
        fputc('\n', stderr);                                                                       \
        abort();                                                                                   \
    } while (0)

#define g_assert_true(expr)                                                                        \
    do {                                                                                           \
        if (!(expr))                                                                               \
            TESTING_FAIL("assert_true(%s)", #expr);                                                 \
    } while (0)
#define g_assert_false(expr)                                                                       \
    do {                                                                                           \
        if (expr)                                                                                  \
            TESTING_FAIL("assert_false(%s)", #expr);                                                \
    } while (0)
#define g_assert_null(expr)                                                                        \
    do {                                                                                           \
        if ((expr) != NULL)                                                                        \
            TESTING_FAIL("assert_null(%s)", #expr);                                                 \
    } while (0)
#define g_assert_nonnull(expr)                                                                     \
    do {                                                                                           \
        if ((expr) == NULL)                                                                        \
            TESTING_FAIL("assert_nonnull(%s)", #expr);                                              \
    } while (0)
#define g_assert_cmpint(a, op, b)                                                                  \
    do {                                                                                           \
        long long _a = (long long)(a), _b = (long long)(b);                                        \
        if (!(_a op _b))                                                                           \
            TESTING_FAIL("%s %s %s (%lld vs %lld)", #a, #op, #b, _a, _b);                           \
    } while (0)
#define g_assert_cmpuint(a, op, b)                                                                 \
    do {                                                                                           \
        unsigned long long _a = (unsigned long long)(a), _b = (unsigned long long)(b);             \
        if (!(_a op _b))                                                                           \
            TESTING_FAIL("%s %s %s (%llu vs %llu)", #a, #op, #b, _a, _b);                           \
    } while (0)
#define g_assert_cmpstr(a, op, b)                                                                  \
    do {                                                                                           \
        const char *_a = (a), *_b = (b);                                                           \
        if (!(strcmp(_a ? _a : "", _b ? _b : "") op 0))                                            \
            TESTING_FAIL("%s %s %s (\"%s\" vs \"%s\")", #a, #op, #b, _a ? _a : "(null)",            \
                         _b ? _b : "(null)");                                                       \
    } while (0)

/* Test helpers (libc only). Returned strings are heap-allocated; free() them. */
char *testing_read_file(const char *path); /* Whole file as a NUL-terminated string, or NULL. */
int testing_tmp_file(const char *tmpl, char **path); /* mkstemp; returns fd, sets *path. */
char *testing_tmp_dir(const char *tmpl);             /* mkdtemp; returns the directory path. */
char *testing_path_join(const char *a, const char *b);
bool testing_regex(const char *pattern, const char *text); /* POSIX extended regex match. */
void testing_srand(uint32_t seed);
uint32_t testing_rand(void);
uint32_t testing_rand_range(uint32_t low, uint32_t high); /* [low, high) */
#endif
