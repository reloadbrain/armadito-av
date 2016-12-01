/***

Copyright (C) 2015, 2016 Teclib'

This file is part of Armadito core.

Armadito core is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Armadito core is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with Armadito core.  If not, see <http://www.gnu.org/licenses/>.

***/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hash.h"

struct hash_table_entry {
	void *key;
	void *value;
};

struct hash_table {
	enum hash_table_type type;
	size_t size;
	struct hash_table_entry *table;
	size_t key_count;
};

#define HASH_DEFAULT_SIZE 64

#define REMOVED ((void *)2)

struct hash_table *hash_table_new(enum hash_table_type t)
{
	struct hash_table *ht;

	ht = malloc(sizeof(struct hash_table));
	ht->type = t;
	ht->size = HASH_DEFAULT_SIZE;
	ht->table = calloc(ht->size, sizeof(struct hash_table_entry));
	ht->key_count = 0;

	return ht;
}

void hash_table_print(struct hash_table *ht)
{
	size_t i;
	const char *fmt = (ht->type == HASH_KEY_STR) ? " key %s value %p\n" : " key %p value %p\n";

	printf("hash table size %ld key count %ld\n", ht->size, ht->key_count);

	for(i = 0; i < ht->size; i++)
		printf(fmt, ht->table[i].key, ht->table[i].value);
}

/* PJW non-cryptographic string hash function */
static uint32_t hash_str(const char *s)
{
	uint32_t h = 0, high;
	const char *p = s;

	while (*p) {
		h = (h << 4) + *p++;
		if (high = h & 0xF0000000)
			h ^= high >> 24;
		h &= ~high;
	}

	return h;
}

/*
 * Various hash functions
 */

/* Multiplication Method pointer hash function */
#define A (0.5 * (2.23606797749978969640 - 1))
static uint32_t hfmult32(struct hash_table *ht, void *p)
{
	uintptr_t k = H_POINTER_TO_INT(p);
	double x = fmod(k * A, 1.0);

	return (uint32_t)(x * ht->size);
}

/* multiplier is a prime close to 2^32 x phi */
static uint32_t himult32(void *p)
{
	uintptr_t k = H_POINTER_TO_INT(p);

	return k * 2654435761;
}

/* for 64 bits, use 11400712997709160919 which is a prime close to 2^64 x phi */
static uint64_t himult64(void *p)
{
	uintptr_t k = H_POINTER_TO_INT(p);

	return k * UINT64_C(11400712997709160919);
}

/* MurmurHash3 based hash functions */
static uint32_t fmix32(void *p)
{
	uintptr_t k = H_POINTER_TO_INT(p);
	uint32_t h = (uint32_t)k;

	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;

	return h;
}

static uint64_t fmix64(void *p)
{
	uintptr_t k = H_POINTER_TO_INT(p);

	k ^= k >> 33;
	k *= UINT64_C(0xff51afd7ed558ccd);
	k ^= k >> 33;
	k *= UINT64_C(0xc4ceb9fe1a85ec53);
	k ^= k >> 33;

	return k;
}

/*
  some results:

  doing 89 insertions in a table of length 128 (89 is 128 x 0.7)

                      id  update  method
                      ++   += 8   random()
  function
  fmix32              28   33     28
  fmix64              30   25     33
  mult_hash            0   42     33
  mult_hash_i          0   72     32
  mult_hash_i64        0   72     28
*/

/* #define hash_pointer(HT, K) fmix32(K) */
/* #define hash_pointer(HT, K) fmix64(K) */
/* #define hash_pointer(HT, K) hfmult32(HT, K) */
/* #define hash_pointer(HT, K) himult32(K) */
#define hash_pointer(HT, K) himult64(K)

#define HASH(HT, K) ((HT->type == HASH_KEY_STR) ? hash_str(K) : hash_pointer(HT, K))
#define EQUAL(HT, K1, K2) ((HT->type == HASH_KEY_STR) ? (strcmp((const char *)K1, (const char *)K2) == 0) : (K1 == K2))

static void hash_table_rehash(struct hash_table *ht)
{
	size_t old_size, j;
	struct hash_table_entry *old_table;

	fprintf(stderr, "rehashing: %ld keys vs. %ld size\n", ht->key_count, ht->size);

	old_size = ht->size;
	old_table = ht->table;
	ht->size = 2 * old_size;
	ht->table = calloc(ht->size, sizeof(struct hash_table_entry));

	for(j = 0; j < old_size; j++) {
		size_t h, i, w;

		h = HASH(ht, old_table[j].key) % ht->size;

		for (i = 0; i < ht->size; i++) {
			w = (h + i) % ht->size;

			if (ht->table[w].key == NULL)
				break;
		}

		ht->table[w].key = old_table[j].key;
		ht->table[w].value = old_table[j].value;
	}

	free(old_table);
}

static int hash_table_must_rehash(struct hash_table *ht)
{
	/* 16 / 23 == 0.6956 e.g. ~ 0.7 */
	return 23 * ht->key_count > 16 * ht->size;
}

static void hash_table_check_overflow(struct hash_table *ht)
{
	if (hash_table_must_rehash(ht))
		hash_table_rehash(ht);
}

int hash_table_insert(struct hash_table *ht, void *key, void *value)
{
	size_t h, i, w;

	hash_table_check_overflow(ht);

	h = HASH(ht, key) % ht->size;

	for (i = 0; i < ht->size; i++) {
		w = (h + i) % ht->size;

		if (ht->table[w].key == NULL || ht->table[w].key == REMOVED)
			break;
	}

	/* this should never happen as we check overflow before inserting */
	if (i == ht->size)
		return 0; /* no NULL place found => table is full */

	if (i != 0)
		fprintf(stderr, "collision for key %p\n", key);

	ht->table[w].key = key;
	ht->table[w].value = value;

	ht->key_count++;

	return 1;
}

static struct hash_table_entry *lookup_entry(struct hash_table *ht, void *key)
{
	size_t h, i, w;

	h = HASH(ht, key) % ht->size;

	for (i = 0; i < ht->size; i++) {
		w = (h + i) % ht->size;

		if (ht->table[w].key == NULL)
			return NULL;

		if (ht->table[w].key == REMOVED)
			continue;

		if (EQUAL(ht, ht->table[w].key, key))
			return ht->table + w;
	}

	return NULL;
}

void *hash_table_search(struct hash_table *ht, void *key)
{
	struct hash_table_entry *p = lookup_entry(ht, key);

	if (p != NULL)
		return p->value;

	return NULL;
}

int hash_table_remove(struct hash_table *ht, void *key, void **p_value)
{
	struct hash_table_entry *p = lookup_entry(ht, key);

	if (p == NULL)
		return 0;

	if (p_value != NULL)
		*p_value = p->value;

	p->key = REMOVED;
	p->value = NULL;

	return 1;
}
