/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides a cache-friendly hash map that
/// uses open addressing with local probing. C99 is needed to compile this for now
/// (restrict is love), otherwise this library has no further requirements.
/// Like, *no* further requirements. At all. Not even libc. I hope this can be of use.
///
/// As this is a single-header library, just include this file whenever you need to
/// access its functionality.
/// IMPORTANT: in at least one file, put
///
///  #define CF_HASHMAP_IMPLEMENTATION
///  #include "cf_hashmap.h"
///
/// before the include, as one compilation unit needs to define the functions exposed
/// by this header.
///
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

/// Calculates the size in bytes for hashmap with estimated `num_elements` entries.
/// The calculated size gives enough space to not go over a 70% load factor.
/// Wikipedia says that 70% are a nice spot before the local probing takes too much time.
/// https://en.wikipedia.org/wiki/Open_addressing
#define CF_HASHMAP_DECENT_BUFFER_SIZE(key_type, value_type, num_elements) \
	_CF_HASHMAP_DECENT_BUFFER_SIZE(sizeof(key_type), sizeof(value_type), (int) (1.5 * num_elements))

/// Provides information and behavior about the key type used in the map.
typedef struct {

	/// Size of a key-value in bytes. You probably should use sizeof() to get it.
	size_t size;

	/// Function pointer to a function that returns true if the keys are the same.
	/// NOTE: the pointers are to the memory holding the value, not the value itself.
	bool (*cmp)(const void * restrict, const void * restrict);

	/// Function pointer to a function that copies a key by reading from one location
	/// and writing to another location. The pointers point to the memory holding the keys,
	/// not the value itself.
	void (*copy)(const void * restrict, void * restrict);
} cf_hashmap_key_info;

/// Provides information about behavior about the value type used in the map.
typedef struct {

	/// Size of a value-value in bytes. You probably should use sizeof() to get it.
	size_t size;

	/// Function pointer to a function that copies a value by reading from one location
	/// and writing to another location. The pointers contain the address of the memory
	/// holding the value, not the value itself.
	void (*copy)(const void * restrict, void * restrict);
} cf_hashmap_value_info;

/// A hashmap type that uses open addressing with local probing.
/// The hashmap uses 4 different regions of memory: flags, hashes, keys and values.
/// Those regions are located next to each other in a buffer in a Struct-of-Arrays
/// kind of fashion.
/// The flags store information about whether the entry is filled or was filled in the past.
/// The hashes are the hashes provided by the user. This hashmap doesn't do any hashing itself.
/// The keys region is used when collisions occur. In such a case the keys will be compared.
/// The values region contains the values. This hashmap really just touches it to store values.
typedef struct {

	/// The number of elements in this hash map.
	size_t num_elements;

	/// The capacity of how many elements *could* be hold in this map.
	size_t capacity;

	size_t key_size;
	size_t value_size;

	bool (*key_cmp)(const void * restrict, const void * restrict);
	void (*key_copy)(const void * restrict, void * restrict);
	void (*value_copy)(const void * restrict, void * restrict);

	void * restrict buffer;

} cf_hashmap;

/// An iterator for iterating over key-value pairs. Use cf_hashmap_iter_start() to acquire such
/// an iterator. Use cf_hashmap_iter_next() to advance the iteration.
typedef struct {
	size_t offset;
} cf_hashmap_iter;


/// This function constructs a new hashmap value. The hashmap uses the information about the
/// key and value type to figure out where to store information in the buffer.
/// The buffer is a chunk of memory that will be used as the storage. It should probably be
/// created by using the `CF_HASHMAP_DECENT_BUFFER_SIZE` macro.
/// Don't modify the contents of the buffer after handing it to a hashmap.
cf_hashmap cf_hashmap_new(cf_hashmap_key_info key_info,
                          cf_hashmap_value_info value_info,
                          size_t buffer_size,
                          void * restrict buffer);

/// Associate a key with a value. The hash is the hash value of the key. This hashmap doesn't
/// perform any hashing itself, so the caller has to provide the hash.
/// For collision resolution, the key itself has to be provided as well.
/// The value pointer points to a memory location that contains the data to be inserted.
void cf_hashmap_set(cf_hashmap * restrict map,
                    uint32_t hash,
                    const void * restrict key,
                    const void * restrict value);

/// Lookup a value in the hashmap. The hash is the hash of the key. The key value itself
/// has to be provided in case a collision occurs.
/// If an entry is found, the value will be written to the out-parameter `value`.
/// Returns true if an entry was found, false otherwise.
bool cf_hashmap_lookup(const cf_hashmap * restrict map,
                       uint32_t hash,
                       const void * restrict key,
                       void * restrict value);

/// Remove an entry from the hashtable by proving the hash and the key value, in case a
/// collision occurs.
void cf_hashmap_remove(cf_hashmap * restrict map,
                       uint32_t hash,
                       const void * restrict key);

/// Create an iterator for the hashmap. Use `cf_hashmap_iter_next` to advance the iteration.
cf_hashmap_iter cf_hashmap_iter_start(const cf_hashmap * restrict map);

/// Advance the iterator to the next elements found. Key and value will be written to the
/// out parameters `key` and `value` in case a next iteration was possible.
/// In that case the function will return true.
/// The function will return false when the end was reached.
bool cf_hashmap_iter_next(const cf_hashmap * restrict map,
                          cf_hashmap_iter * restrict iter,
                          void * restrict key,
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
                          void * restrict buffer)
{
	cf_hashmap _map;
	_map.key_size = key_info.size;
	_map.key_cmp = key_info.cmp;
	_map.key_copy = key_info.copy;

	_map.value_size = value_info.size;
	_map.value_copy = value_info.copy;

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


void cf_hashmap_set(cf_hashmap * restrict map,
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

bool cf_hashmap_lookup(const cf_hashmap * restrict map,
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

	// oh. We searched through the whole table, that means every entry was
	// filled at *some point*, which is still pretty scary. The user should
	// switch to a new table with a bigger buffer
	return false;

}

void cf_hashmap_remove(cf_hashmap * restrict map,
                       uint32_t hash,
                       const void * restrict key)
{
	_CF_HASHMAP_PROLOGUE();
	(void) values_ptr;

	for (size_t i = 0; i < capacity; i++) {
		size_t pos = (hash + i) % capacity;

		size_t flag_pos = pos / 4;
		size_t flag_pos_offset = pos % 4;

		bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
		bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

		if (is_filled) {
			if (hashes_ptr[pos] == hash && map->key_cmp(&keys_ptr[pos * map->key_size], key)) {
				// ayyyy, this is the entry, which means we get to kick it out now >:D
				flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset));
				flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset + 1));

				map->num_elements--;

				return;
			}
			continue;
		} else if (was_deleted) {
			continue;
		} else {
			// The entry is not filled but it also wasn't deleted
			// in the past, so we found a hole. That means our value
			// isn't in the table at all. Nothing to remove :'(
			return;
		}
	}
}

cf_hashmap_iter cf_hashmap_iter_start(const cf_hashmap * restrict map)
{
	cf_hashmap_iter iter = {};

	iter.offset = 0;

	return iter;
}

bool cf_hashmap_iter_next(const cf_hashmap * restrict map,
                          cf_hashmap_iter * restrict iter,
                          void * restrict key,
                          void * restrict value)
{

	_CF_HASHMAP_PROLOGUE();
	(void) values_ptr;

	for (size_t i = iter->offset; i < capacity; i++) {
		size_t pos = i;

		size_t flag_pos = pos / 4;
		size_t flag_pos_offset = pos % 4;

		bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

		if (is_filled) {
			// heyyy, we found something, let's write the offset to the iterator and
			// copy the key-value pair!

			iter->offset = i + 1; // next time check the next entry

			map->key_copy(&keys_ptr[pos * map->key_size], key);
			map->value_copy(&values_ptr[pos * map->value_size], value);

			return true;

		} else {
			continue;
		}
	}

	// We're done. If we set the iterator to capacity then next time the loop
	// won't even do one iteration.
	iter->offset = capacity;
	return false;
}

#undef _CF_HASHMAP_PROLOGUE

#endif


#endif
