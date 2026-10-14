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

#ifndef HASHMAP_H
#define HASHMAP_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Open-addressing hash map replacing GLib's GHashTable. Keys and values are void*; the
 * optional destructors own them. It keys either small integers packed into a pointer, or
 * C strings, through the provided hash/equal functions. Allocation failures abort. */
typedef uint32_t (*hashmap_hash_fn)(const void *key);
typedef bool (*hashmap_equal_fn)(const void *a, const void *b);
typedef void (*hashmap_free_fn)(void *pointer);

struct hashmap;

struct hashmap *hashmap_new(hashmap_hash_fn hash, hashmap_equal_fn equal, hashmap_free_fn key_free,
                            hashmap_free_fn value_free);
void hashmap_destroy(struct hashmap *map); /* Frees every key/value via the destructors. */
void *hashmap_lookup(struct hashmap *map, const void *key);
bool hashmap_contains(struct hashmap *map, const void *key);
/* Insert or replace. Takes ownership of key. On replace, frees the new key and old value. */
void hashmap_insert(struct hashmap *map, void *key, void *value);
bool hashmap_remove(struct hashmap *map, const void *key); /* Frees the stored key/value. */
void hashmap_remove_all(struct hashmap *map);
size_t hashmap_size(const struct hashmap *map);
size_t hashmap_capacity(const struct hashmap *map); /* Slot count; exposed for tests. */

/* Iteration. Removing the entry just returned with hashmap_iter_remove() is safe; inserts
 * during iteration are not. */
struct hashmap_iter {
    struct hashmap *map;
    size_t slot;
    size_t current;
    bool valid;
};

void hashmap_iter_init(struct hashmap_iter *it, struct hashmap *map);
bool hashmap_iter_next(struct hashmap_iter *it, void **key, void **value);
void hashmap_iter_remove(struct hashmap_iter *it);

/* Snapshot of the live keys; caller frees the array with free(). Keys stay owned by map. */
void **hashmap_keys(struct hashmap *map, size_t *count);

uint32_t hashmap_hash_u32(const void *key); /* key == (void*)(uintptr_t)value */
bool hashmap_equal_u32(const void *a, const void *b);
uint32_t hashmap_hash_str(const void *key); /* key == (const char *) */
bool hashmap_equal_str(const void *a, const void *b);
#endif
