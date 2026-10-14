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

#include "hashmap.h"

#include "compat.h"

#include <stdlib.h>
#include <string.h>

enum slot_state { SLOT_EMPTY = 0, SLOT_OCCUPIED, SLOT_DELETED };

struct slot {
    void *key;
    void *value;
    uint32_t hash;
    uint8_t state;
};

struct hashmap {
    struct slot *slots;
    size_t cap;      /* Always a power of two. */
    size_t count;    /* Live (occupied) entries. */
    size_t occupied; /* Occupied + deleted, for the load factor. */
    hashmap_hash_fn hash;
    hashmap_equal_fn equal;
    hashmap_free_fn key_free;
    hashmap_free_fn value_free;
};

#define HASHMAP_MIN_CAP 16u

struct hashmap *hashmap_new(hashmap_hash_fn hash, hashmap_equal_fn equal, hashmap_free_fn key_free,
                            hashmap_free_fn value_free)
{
    struct hashmap *map = xcalloc(1, sizeof(*map));
    map->cap = HASHMAP_MIN_CAP;
    map->slots = xcalloc(map->cap, sizeof(*map->slots));
    map->hash = hash;
    map->equal = equal;
    map->key_free = key_free;
    map->value_free = value_free;
    return map;
}

static void free_entry(struct hashmap *map, struct slot *s)
{
    if (map->key_free)
        map->key_free(s->key);
    if (map->value_free)
        map->value_free(s->value);
}

void hashmap_destroy(struct hashmap *map)
{
    if (!map)
        return;
    for (size_t i = 0; i < map->cap; i++)
        if (map->slots[i].state == SLOT_OCCUPIED)
            free_entry(map, &map->slots[i]);
    free(map->slots);
    free(map);
}

/* Find the slot holding key, or SIZE_MAX. When inserting, also report the first reusable
 * (empty or deleted) slot so a replace reuses a tombstone. */
static size_t find(struct hashmap *map, const void *key, uint32_t hash, size_t *insert_at)
{
    size_t mask = map->cap - 1;
    size_t i = hash & mask;
    size_t reusable = SIZE_MAX;
    for (size_t probed = 0; probed < map->cap; probed++) {
        struct slot *s = &map->slots[i];
        if (s->state == SLOT_EMPTY) {
            if (insert_at)
                *insert_at = reusable != SIZE_MAX ? reusable : i;
            return SIZE_MAX;
        }
        if (s->state == SLOT_DELETED) {
            if (reusable == SIZE_MAX)
                reusable = i;
        } else if (s->hash == hash && map->equal(s->key, key)) {
            return i;
        }
        i = (i + 1) & mask;
    }
    if (insert_at)
        *insert_at = reusable;
    return SIZE_MAX;
}

static void resize(struct hashmap *map, size_t new_cap)
{
    struct slot *old = map->slots;
    size_t old_cap = map->cap;
    map->slots = xcalloc(new_cap, sizeof(*map->slots));
    map->cap = new_cap;
    map->occupied = map->count;
    size_t mask = new_cap - 1;
    for (size_t i = 0; i < old_cap; i++) {
        if (old[i].state != SLOT_OCCUPIED)
            continue;
        size_t j = old[i].hash & mask;
        while (map->slots[j].state == SLOT_OCCUPIED)
            j = (j + 1) & mask;
        map->slots[j] = old[i];
    }
    free(old);
}

void *hashmap_lookup(struct hashmap *map, const void *key)
{
    size_t i = find(map, key, map->hash(key), NULL);
    return i == SIZE_MAX ? NULL : map->slots[i].value;
}

bool hashmap_contains(struct hashmap *map, const void *key)
{
    return find(map, key, map->hash(key), NULL) != SIZE_MAX;
}

void hashmap_insert(struct hashmap *map, void *key, void *value)
{
    /* Rehash before inserting so there is always an empty slot to terminate probing. Double
     * only when live entries need the room; otherwise the load is mostly tombstones, so
     * rehash at the same size. Without this, churn of distinct keys would grow memory
     * without bound even though few entries are live. */
    if ((map->occupied + 1) * 4 >= map->cap * 3)
        resize(map, (map->count + 1) * 2 > map->cap ? map->cap * 2 : map->cap);
    uint32_t hash = map->hash(key);
    size_t insert_at;
    size_t found = find(map, key, hash, &insert_at);
    if (found != SIZE_MAX) {
        if (map->key_free)
            map->key_free(key);
        if (map->value_free)
            map->value_free(map->slots[found].value);
        map->slots[found].value = value;
        return;
    }
    struct slot *s = &map->slots[insert_at];
    if (s->state == SLOT_EMPTY)
        map->occupied++;
    s->key = key;
    s->value = value;
    s->hash = hash;
    s->state = SLOT_OCCUPIED;
    map->count++;
}

bool hashmap_remove(struct hashmap *map, const void *key)
{
    size_t i = find(map, key, map->hash(key), NULL);
    if (i == SIZE_MAX)
        return false;
    free_entry(map, &map->slots[i]);
    map->slots[i].key = NULL;
    map->slots[i].value = NULL;
    map->slots[i].state = SLOT_DELETED;
    map->count--;
    return true;
}

void hashmap_remove_all(struct hashmap *map)
{
    for (size_t i = 0; i < map->cap; i++)
        if (map->slots[i].state == SLOT_OCCUPIED) {
            free_entry(map, &map->slots[i]);
            map->slots[i].state = SLOT_DELETED;
        }
    map->count = 0;
    /* Reset to a clean, compact table so stale tombstones do not slow later probing. */
    free(map->slots);
    map->cap = HASHMAP_MIN_CAP;
    map->slots = xcalloc(map->cap, sizeof(*map->slots));
    map->occupied = 0;
}

size_t hashmap_size(const struct hashmap *map)
{
    return map->count;
}

size_t hashmap_capacity(const struct hashmap *map)
{
    return map->cap;
}

void hashmap_iter_init(struct hashmap_iter *it, struct hashmap *map)
{
    it->map = map;
    it->slot = 0;
    it->current = SIZE_MAX;
    it->valid = false;
}

bool hashmap_iter_next(struct hashmap_iter *it, void **key, void **value)
{
    struct hashmap *map = it->map;
    while (it->slot < map->cap) {
        size_t i = it->slot++;
        if (map->slots[i].state == SLOT_OCCUPIED) {
            it->current = i;
            it->valid = true;
            if (key)
                *key = map->slots[i].key;
            if (value)
                *value = map->slots[i].value;
            return true;
        }
    }
    it->valid = false;
    return false;
}

void hashmap_iter_remove(struct hashmap_iter *it)
{
    if (!it->valid)
        return;
    struct hashmap *map = it->map;
    struct slot *s = &map->slots[it->current];
    free_entry(map, s);
    s->key = NULL;
    s->value = NULL;
    s->state = SLOT_DELETED;
    map->count--;
    it->valid = false;
}

void **hashmap_keys(struct hashmap *map, size_t *count)
{
    void **keys = xcalloc(map->count ? map->count : 1, sizeof(*keys));
    size_t n = 0;
    for (size_t i = 0; i < map->cap; i++)
        if (map->slots[i].state == SLOT_OCCUPIED)
            keys[n++] = map->slots[i].key;
    *count = n;
    return keys;
}

static uint32_t mix32(uint32_t h)
{
    h ^= h >> 16;
    h *= 0x85ebca6bu;
    h ^= h >> 13;
    h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

uint32_t hashmap_hash_u32(const void *key)
{
    return mix32((uint32_t)(uintptr_t)key);
}

bool hashmap_equal_u32(const void *a, const void *b)
{
    return a == b;
}

uint32_t hashmap_hash_str(const void *key)
{
    const unsigned char *s = key;
    uint32_t h = 2166136261u; /* FNV-1a */
    while (*s) {
        h ^= *s++;
        h *= 16777619u;
    }
    return h;
}

bool hashmap_equal_str(const void *a, const void *b)
{
    return strcmp(a, b) == 0;
}
