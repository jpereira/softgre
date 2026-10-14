/* Model-based libFuzzer harness: every hashmap operation is mirrored in a reference array
 * and compared, while ASan, UBSan and LeakSanitizer watch for memory errors and leaks. */
#include "compat.h"
#include "hashmap.h"

#include <stdio.h>

#define KEYS 24

static void fail(const char *what)
{
    fprintf(stderr, "hashmap model mismatch: %s\n", what);
    abort();
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    struct hashmap *map = hashmap_new(hashmap_hash_str, hashmap_equal_str, free, NULL);
    uintptr_t model[KEYS] = {0};
    size_t live = 0;
    for (size_t i = 0; i + 1 < size; i += 2) {
        unsigned op = data[i] % 5, k = data[i + 1] % KEYS;
        char key[8];
        snprintf(key, sizeof(key), "k%u", k);
        if (op == 0) {
            uintptr_t value = (uintptr_t)(data[i] | 0x100);
            hashmap_insert(map, xstrdup(key), (void *)value);
            live += model[k] ? 0 : 1;
            model[k] = value;
        } else if (op == 1) {
            if (hashmap_remove(map, key) != (model[k] != 0))
                fail("remove");
            live -= model[k] ? 1 : 0;
            model[k] = 0;
        } else if (op == 2) {
            if ((uintptr_t)hashmap_lookup(map, key) != model[k] ||
                hashmap_contains(map, key) != (model[k] != 0))
                fail("lookup");
        } else if (op == 3) {
            unsigned seen[KEYS] = {0};
            struct hashmap_iter it;
            void *ek, *ev;
            hashmap_iter_init(&it, map);
            while (hashmap_iter_next(&it, &ek, &ev)) {
                unsigned idx = (unsigned)atoi((char *)ek + 1);
                if (idx >= KEYS || (uintptr_t)ev != model[idx] || seen[idx]++)
                    fail("iterate");
            }
        } else if (k == 0) {
            hashmap_remove_all(map);
            memset(model, 0, sizeof(model));
            live = 0;
        }
        if (hashmap_size(map) != live)
            fail("size");
    }
    hashmap_destroy(map);
    return 0;
}
