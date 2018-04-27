/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef CF_HASHMAP_H
#define CF_HASHMAP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define _CF_HASHMAP_DECENT_BUFFER_SIZE(key_size, value_size, capacity) \
	( \
	        (sizeof(uint32_t) + key_size + value_size) * capacity \
	        + (capacity / 4) + 1 \
	)

#define CF_HASHMAP_DECENT_BUFFER_SIZE(key_type, value_type, num_elements) \
	_CF_HASHMAP_DECENT_BUFFER_SIZE(sizeof(key_type), sizeof(value_type), (int) (1.5 * num_elements))


typedef struct {
	size_t size;
	bool (*cmp)(const void * restrict, const void * restrict);
	void (*copy)(const void * restrict, void * restrict);
} cf_hashmap_key_info;

typedef struct {
	size_t size;
	void (*copy)(const void * restrict, void * restrict);
} cf_hashmap_value_info;

typedef struct {
	size_t num_elements;
	size_t capacity;

	size_t key_size;
	size_t value_size;

	size_t buffer_size;

	bool (*key_cmp)(const void * restrict, const void * restrict);
	void (*key_copy)(const void * restrict, void * restrict);
	void (*value_copy)(const void * restrict, void * restrict);

	void *buffer;

} cf_hashmap;


cf_hashmap cf_hashmap_new(cf_hashmap_key_info key_info,
                          cf_hashmap_value_info value_info,
                          size_t buffer_size,
                          void *buffer);

void cf_hashmap_set(cf_hashmap *map,
                    uint32_t hash,
                    const void * restrict key,
                    const void * restrict value);

bool cf_hashmap_lookup(const cf_hashmap *map,
                       uint32_t hash,
                       const void * restrict key,
                       void * restrict value);


#ifdef CF_HASHMAP_IMPLEMENTATION

#define _CF_HASHMAP_PROLOGUE() \
	size_t capacity = map->capacity; \
	uint8_t * restrict flags_ptr = map->buffer; \
	uint32_t * restrict hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1)); \
	uint8_t * restrict keys_ptr = (uint8_t *)hashes_ptr + sizeof(uint32_t) * capacity; \
	uint8_t * restrict values_ptr = (uint8_t *)keys_ptr + map->key_size * capacity; \

cf_hashmap cf_hashmap_new(cf_hashmap_key_info key_info,
                          cf_hashmap_value_info value_info,
                          size_t buffer_size,
                          void *buffer)
{
	cf_hashmap _map;
	_map.key_size = key_info.size;
	_map.key_cmp = key_info.cmp;
	_map.key_copy = key_info.copy;

	_map.value_size = value_info.size;
	_map.value_copy = value_info.copy;

	_map.buffer_size = buffer_size;

	_map.buffer = buffer;

	_map.num_elements = 0;
	_map.capacity = (size_t)((float)(buffer_size - 1) / (key_info.size + value_info.size + sizeof(uint32_t) + 1/4.0));

	cf_hashmap *map = &_map;

	_CF_HASHMAP_PROLOGUE();
	(void) values_ptr;


	// okay, now that this is all set up, let's initialize the buffer

	for (size_t i = 0; i < (capacity / 4 + 1); i++) {
		flags_ptr[i] = 0;
	}

	for (size_t i = 0; i < capacity; i++) {
		hashes_ptr[i] = 0;
	}

	return _map;
}


void cf_hashmap_set(cf_hashmap *map,
                    uint32_t hash,
                    const void * restrict key,
                    const void * restrict value)
{
	_CF_HASHMAP_PROLOGUE();

	for (size_t i = 0; i < capacity; i++) {
		size_t pos = (hash + i) % capacity;

		size_t flag_pos = pos / 4;
		size_t flag_pos_offset = pos % 4;

		bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

		if (is_filled) {
			if (hashes_ptr[pos] == hash && map->key_cmp(&keys_ptr[pos * map->key_size], key)) {
				// entry already exists! set the value and then we're done here.
				map->value_copy(value, &values_ptr[pos * map->value_size]);
				return;
			}
			continue;
		}

		hashes_ptr[pos] = hash;
		map->key_copy(key, &keys_ptr[pos * map->key_size]);
		map->value_copy(value, &values_ptr[pos * map->value_size]);

		// set the filled flag
		flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset));
		// also unset the "deleted" flag... you never know!
		flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset + 1));

		map->num_elements++;
		return;
	}
}

bool cf_hashmap_lookup(const cf_hashmap *map,
                       uint32_t hash,
                       const void * restrict key,
                       void * restrict value)
{
	_CF_HASHMAP_PROLOGUE();

	for (size_t i = 0; i < capacity; i++) {
		size_t pos = (hash + i) % capacity;

		size_t flag_pos = pos / 4;
		size_t flag_pos_offset = pos % 4;

		bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
		bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

		if (is_filled) {
			if (hashes_ptr[pos] == hash && map->key_cmp(&keys_ptr[pos * map->key_size], key)) {
				// entry exists! write to the out-parameter and say that we found it!
				map->value_copy(&values_ptr[pos * map->value_size], value);
				return true;
			}
			continue;
		} else if (was_deleted) {
			continue;
		} else {
			// The entry is not filled but it also wasn't deleted
			// in the past, so we found a hole. That means our value
			// isn't in the table at all.
			return false;
		}
	}

	return false;

}


#undef _CF_HASHMAP_PROLOGUE

#endif


#endif
