/* Implementation of the minimal test harness in testing.h. */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "testing.h"

#include "compat.h"

#include <regex.h>
#include <stdlib.h>

#define TESTING_MAX 64

static struct {
    const char *name;
    test_func fn;
} registry[TESTING_MAX];

static size_t registered;
const char *testing_current = "";

void testing_add(const char *name, test_func fn)
{
    if (registered < TESTING_MAX) {
        registry[registered].name = name;
        registry[registered].fn = fn;
        registered++;
    }
}

int testing_run(void)
{
    for (size_t i = 0; i < registered; i++) {
        testing_current = registry[i].name;
        registry[i].fn();
        printf("ok %s\n", registry[i].name);
    }
    printf("# %zu tests passed\n", registered);
    return 0;
}

char *testing_read_file(const char *path)
{
    FILE *file = fopen(path, "rb");
    if (!file)
        return NULL;
    size_t cap = 4096, len = 0;
    char *buf = xmalloc(cap);
    size_t got;
    while ((got = fread(buf + len, 1, cap - len - 1, file)) > 0) {
        len += got;
        if (len + 1 == cap) {
            cap *= 2;
            buf = xrealloc(buf, cap);
        }
    }
    buf[len] = '\0';
    fclose(file);
    return buf;
}

static const char *tmp_root(void)
{
    const char *dir = getenv("TMPDIR");
    return dir && *dir ? dir : "/tmp";
}

int testing_tmp_file(const char *tmpl, char **path)
{
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/%s", tmp_root(), tmpl);
    int fd = mkstemp(buf);
    *path = fd >= 0 ? xstrdup(buf) : NULL;
    return fd;
}

char *testing_tmp_dir(const char *tmpl)
{
    char buf[4096];
    snprintf(buf, sizeof(buf), "%s/%s", tmp_root(), tmpl);
    return mkdtemp(buf) ? xstrdup(buf) : NULL;
}

char *testing_path_join(const char *a, const char *b)
{
    return xstrdup_printf("%s/%s", a, b);
}

bool testing_regex(const char *pattern, const char *text)
{
    regex_t re;
    if (regcomp(&re, pattern, REG_EXTENDED | REG_NOSUB))
        return false;
    bool matched = regexec(&re, text, 0, NULL, 0) == 0;
    regfree(&re);
    return matched;
}

static uint32_t rng_state = 0x9e3779b9u;

void testing_srand(uint32_t seed)
{
    rng_state = seed ? seed : 0x9e3779b9u;
}

uint32_t testing_rand(void)
{
    /* xorshift32: deterministic pseudo-random bytes for exercising the parser. */
    rng_state ^= rng_state << 13;
    rng_state ^= rng_state >> 17;
    rng_state ^= rng_state << 5;
    return rng_state;
}

uint32_t testing_rand_range(uint32_t low, uint32_t high)
{
    return high <= low ? low : low + testing_rand() % (high - low);
}
