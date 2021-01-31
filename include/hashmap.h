#ifndef HASHMAP_H
#define HASHMAP_H

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const uint32_t HASHMAP_DEFAULT_BUCKET_COUNT = 1048576;

struct HashmapNode {
        uint8_t exists;
        const void* key;
        uint32_t value;
        struct HashmapNode* next;
};

struct Hashmap {
        uint32_t bucket_ct;
	struct HashmapNode* buckets;
};

void hashmap_create(struct Hashmap* hashmap) {
        hashmap->bucket_ct = HASHMAP_DEFAULT_BUCKET_COUNT;
        hashmap->buckets = malloc(hashmap->bucket_ct * sizeof(hashmap->buckets[0]));
        for (int i = 0; i < hashmap->bucket_ct; i++) {
                hashmap->buckets[i].exists = 0;
                hashmap->buckets[i].next = NULL;
        }
}

void hashmap_destroy(struct Hashmap* hashmap) {
        free(hashmap->buckets);
}

void hashmap_insert(struct Hashmap* hashmap,
                    uint32_t (*hasher)(uint32_t bucket_ct, const void* key),
                    const void* key, uint32_t value)
{
        uint32_t hash = hasher(hashmap->bucket_ct, key);
        assert(hash < hashmap->bucket_ct);
        struct HashmapNode* node = &hashmap->buckets[hash];

        while (node != NULL && node->exists && node->next != NULL) node = node->next;
        if (node->exists) {
                node->next = malloc(sizeof(*node->next));
                node = node->next;
                node->exists = 0;
                node->next = NULL;
        }

        node->exists = 1;
        node->key = key;
        node->value = value;
}

// Returns 0 if the key doesn't exist, 1 if it does. If it exists, assigns to [value]
int hashmap_get(struct Hashmap* hashmap,
                uint32_t (*hasher)(uint32_t bucket_ct, const void* key),
                int (*compare)(const void* a, const void* b),
                const void* key, uint32_t* value)
{
        uint32_t hash = hasher(hashmap->bucket_ct, key);
        assert(hash < hashmap->bucket_ct);
        if (!hashmap->buckets[hash].exists) return 0;
        struct HashmapNode* node = &hashmap->buckets[hash];

        while (node != NULL && compare(key, node->key) != 0) node = node->next;

        if (node != NULL) {
                assert(compare(key, node->key) == 0);
                *value = node->value;
                return 1;
        }

        return 0;
}

#endif // HASHMAP_H
