/* The in-tree hash map replaces GLib's GHashTable: check it against a reference model and
 * for the properties the daemon relies on (ownership, removal during iteration, bounded
 * memory under churn). */
#include "compat.h"
#include "hashmap.h"
#include "testing.h"

#define KEYS 200

static unsigned freed_keys, freed_values;

static void count_key(void *key)
{
    freed_keys++;
    free(key);
}

static void count_value(void *value)
{
    (void)value;
    freed_values++;
}

static char *key_of(unsigned k)
{
    return xstrdup_printf("02:00:00:00:%02x:%02x", k >> 8, k & 255);
}

static void test_model(void)
{
    struct hashmap *map = hashmap_new(hashmap_hash_str, hashmap_equal_str, free, NULL);
    uintptr_t model[KEYS] = {0};
    size_t live = 0;
    testing_srand(7);
    for (unsigned step = 0; step < 200000; step++) {
        unsigned k = testing_rand_range(0, KEYS), op = testing_rand_range(0, 100);
        char *key = key_of(k);
        if (op < 40) {
            uintptr_t value = 1 + testing_rand_range(0, 1000);
            hashmap_insert(map, xstrdup(key), (void *)value);
            live += model[k] ? 0 : 1;
            model[k] = value;
        } else if (op < 70) {
            bool removed = hashmap_remove(map, key);
            g_assert_true(removed == (model[k] != 0));
            live -= model[k] ? 1 : 0;
            model[k] = 0;
        } else if (op < 99) {
            g_assert_true((uintptr_t)hashmap_lookup(map, key) == model[k]);
            g_assert_true(hashmap_contains(map, key) == (model[k] != 0));
        } else {
            hashmap_remove_all(map);
            memset(model, 0, sizeof(model));
            live = 0;
        }
        g_assert_cmpuint(hashmap_size(map), ==, live);
        free(key);
    }
    hashmap_destroy(map);
}

static void test_iteration_removal(void)
{
    freed_keys = freed_values = 0;
    struct hashmap *map = hashmap_new(hashmap_hash_str, hashmap_equal_str, count_key, count_value);
    for (unsigned i = 0; i < 100; i++)
        hashmap_insert(map, key_of(i), (void *)(uintptr_t)(i + 1));
    /* Remove the even entries while iterating; the odd ones must all survive exactly once. */
    unsigned visited = 0;
    struct hashmap_iter it;
    void *key, *value;
    hashmap_iter_init(&it, map);
    while (hashmap_iter_next(&it, &key, &value)) {
        visited++;
        if ((uintptr_t)value % 2 == 1)
            hashmap_iter_remove(&it);
    }
    g_assert_cmpuint(visited, ==, 100);
    g_assert_cmpuint(hashmap_size(map), ==, 50);
    g_assert_cmpuint(freed_keys, ==, 50);
    g_assert_cmpuint(freed_values, ==, 50);
    unsigned remaining = 0;
    hashmap_iter_init(&it, map);
    while (hashmap_iter_next(&it, &key, &value)) {
        g_assert_cmpuint((uintptr_t)value % 2, ==, 0);
        remaining++;
    }
    g_assert_cmpuint(remaining, ==, 50);
    /* Replacing a key frees the duplicate key and the old value, never the stored key. */
    freed_keys = freed_values = 0;
    char *dup = key_of(1);
    hashmap_insert(map, dup, (void *)(uintptr_t)2);
    g_assert_cmpuint(freed_keys, ==, 1);
    g_assert_cmpuint(freed_values, ==, 1);
    g_assert_cmpuint(hashmap_size(map), ==, 50);
    hashmap_destroy(map);
    g_assert_cmpuint(freed_keys, ==, 51);
    g_assert_cmpuint(freed_values, ==, 51);
}

/* Inserting and removing many distinct keys while few are live must not grow the table. */
static void test_bounded_under_churn(void)
{
    struct hashmap *map = hashmap_new(hashmap_hash_str, hashmap_equal_str, free, NULL);
    size_t peak = 0;
    for (unsigned i = 0; i < 100000; i++) {
        char *key = key_of(i);
        hashmap_insert(map, xstrdup(key), (void *)1);
        if (i >= 8) {
            char *old = key_of(i - 8);
            g_assert_true(hashmap_remove(map, old));
            free(old);
        }
        free(key);
        if (hashmap_capacity(map) > peak)
            peak = hashmap_capacity(map);
    }
    g_assert_cmpuint(hashmap_size(map), ==, 8);
    g_assert_cmpuint(peak, <=, 64);
    hashmap_destroy(map);
}

static void test_integer_keys_and_keys_snapshot(void)
{
    struct hashmap *map = hashmap_new(hashmap_hash_u32, hashmap_equal_u32, NULL, NULL);
    for (uintptr_t k = 1; k <= 5000; k++)
        hashmap_insert(map, (void *)k, (void *)(k * 2));
    g_assert_cmpuint(hashmap_size(map), ==, 5000);
    g_assert_true((uintptr_t)hashmap_lookup(map, (void *)4999) == 9998);
    g_assert_false(hashmap_contains(map, (void *)0x7fffffffu));
    size_t count;
    void **keys = hashmap_keys(map, &count);
    g_assert_cmpuint(count, ==, 5000);
    free(keys);
    hashmap_remove_all(map);
    g_assert_cmpuint(hashmap_size(map), ==, 0);
    g_assert_cmpuint(hashmap_capacity(map), ==, 16);
    hashmap_destroy(map);
}

int main(int argc, char **argv)
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func("/hashmap/model", test_model);
    g_test_add_func("/hashmap/iteration-removal-and-ownership", test_iteration_removal);
    g_test_add_func("/hashmap/bounded-under-churn", test_bounded_under_churn);
    g_test_add_func("/hashmap/integer-keys-and-snapshot", test_integer_keys_and_keys_snapshot);
    return g_test_run();
}
