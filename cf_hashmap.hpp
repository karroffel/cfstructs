/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides a cache-friendly hash map that
/// uses open addressing with local probing. This hashmap doesn't perform hashing
/// itself, also it can only be used with POD types. Key comparison uses operator==, so
/// this might need to be implemented for the key type if it is not a primitive type.
///
#ifndef CF_HASHMAP_HPP
#define CF_HASHMAP_HPP

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

namespace cf {

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

/// A hashmap type that uses open addressing with local probing.
/// The hashmap uses 4 different regions of memory: flags, hashes, keys and values.
/// Those regions are located next to each other in a buffer in a Struct-of-Arrays
/// kind of fashion.
/// The flags store information about whether the entry is filled or was filled in the past.
/// The hashes are the hashes provided by the user. This hashmap doesn't do any hashing itself.
/// The keys region is used when collisions occur. In such a case the keys will be compared.
/// The values region contains the values. This hashmap really just touches it to store values.
template <typename TKey, typename TValue>
struct hashmap {

	/// The number of elements in this hash map.
	size_t num_elements;

	/// The capacity of how many elements *could* be hold in this map.
	size_t capacity;

	void *buffer;

	/// An iterator for iterating over key-value pairs. Use `iter_start()` to acquire
	/// such an iterator. Use `iter_next()` to advance the iteration.
	struct iter {
		size_t offset;
	};

	/// This function constructs a new hashmap value.
	/// The buffer is a chunk of memory that will be used as the storage. It should probably be
	/// created by using the `CF_HASHMAP_DECENT_BUFFER_SIZE` macro.
	/// Don't modify the contents of the buffer after handing it to a hashmap.
	static hashmap create(size_t buffer_size, void *buffer)
	{
		hashmap<TKey, TValue> map = {};

		map.buffer = buffer;
		map.num_elements = 0;
		map.capacity = (size_t)((float)(buffer_size - 1) / (sizeof(TKey) + sizeof(TValue) + sizeof(uint32_t) + 1/4.0));

		size_t capacity = map.capacity;
		uint8_t *flags_ptr = (uint8_t *) map.buffer;

		// okay, now that this is all set up, let's initialize the buffer
		for (size_t i = 0; i < (capacity / 4 + 1); i++) {
			flags_ptr[i] = 0;
		}

		return map;
	}

	/// Associate a key with a value. The hash is the hash value of the key. This hashmap doesn't
	/// perform any hashing itself, so the caller has to provide the hash.
	/// For collision resolution, the key itself has to be provided as well.
	void set(uint32_t hash, const TKey &key, const TValue &value)
	{
		const size_t capacity = this->capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		TKey *keys_ptr = (TKey *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);
		TValue *values_ptr = (TValue *) ((uint8_t *) keys_ptr + sizeof(TKey) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && keys_ptr[pos] == key) {
					// entry already exists! set the value and then we're done here.
					values_ptr[pos] = value;
					return;
				}
				continue;
			}

			hashes_ptr[pos] = hash;
			keys_ptr[pos] = key;
			values_ptr[pos] = value;

			// set the filled flag
			flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset));
			// also unset the "deleted" flag... you never know!
			flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset + 1));

			num_elements++;
			return;
		}
	}

	/// Lookup a value in the hashmap. The hash is the hash of the key. The key value itself
	/// has to be provided in case a collision occurs.
	/// If an entry is found, the value will be written to the out-parameter `value`.
	/// Returns true if an entry was found, false otherwise.
	bool lookup(uint32_t hash, const TKey &key, TValue &value) const
	{
		const size_t capacity = this->capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		TKey *keys_ptr = (TKey *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);
		TValue *values_ptr = (TValue *) ((uint8_t *) keys_ptr + sizeof(TKey) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
			bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && keys_ptr[pos] == key) {
					// entry exists! write to the out-parameter and say that we found it!
					value = values_ptr[pos];
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

	/// Get the value associated with the given hash and key.
	/// This uses `lookup()` internally, but discards the bool return value
	/// and returns the value instead of having it as an out parameter.
	/// WARNING: If the entry was not found then the return value is **undefined**.
	inline TValue get(uint32_t hash, const TKey &key) const
	{
		TValue value;
		lookup(hash, key, value);
		return value;
	}

	/// Remove an entry from the hashtable by proving the hash and the key value, in case a
	/// collision occurs.
	void remove(uint32_t hash, const TKey &key)
	{
		const size_t capacity = this->capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		TKey *keys_ptr = (TKey *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
			bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && keys_ptr[pos] == key) {
					// ayyyy, this is the entry, which means we get to kick it out now >:D
					flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset));
					flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset + 1));

					num_elements--;
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

	/// Create an iterator for the hashmap. Use `iter_next()` to advance the iteration.
	iter iter_start() const
	{
		hashmap<TKey, TValue>::iter iter = {};
		iter.offset = 0;
		return iter;
	}

	/// Advance the iterator to the next elements found. Key and value will be written to the
	/// out parameters `key` and `value` in case a next iteration was possible.
	/// In that case the function will return true.
	/// The function will return false when the end was reached.
	bool iter_next(iter &iter, TKey &key, TValue &value) const
	{
		const size_t capacity = this->capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		TKey *keys_ptr = (TKey *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);
		TValue *values_ptr = (TValue *) ((uint8_t *) keys_ptr + sizeof(TKey) * capacity);

		for (size_t i = iter.offset; i < capacity; i++) {
			size_t pos = i;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				// heyyy, we found something, let's write the offset to the iterator and
				// copy the key-value pair!

				iter.offset = i + 1; // next time check the next entry

				key = keys_ptr[pos];
				value = values_ptr[pos];

				return true;

			} else {
				continue;
			}
		}

		// We're done. If we set the iterator to capacity then next time the loop
		// won't even do one iteration.
		iter.offset = capacity;
		return false;
	}

	/// Calculates the load factor of the map. When the load factor is greater than 0.7
	/// then `copy()` should be used to relocate the hashmap for better performance.
	inline float load_factor() const
	{
		return num_elements / (float) capacity;
	}

	/// Creates a new hashmap using a different buffer. All the entries of the
	/// current map will be inserted into the new map.
	hashmap<TKey, TValue> copy(size_t buffer_size, void *buffer) const
	{
		const size_t capacity = this->capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		TKey *keys_ptr = (TKey *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);
		TValue *values_ptr = (TValue *) ((uint8_t *) keys_ptr + sizeof(TKey) * capacity);

		// create the new hashmap
		hashmap<TKey, TValue> new_hashmap;
		new_hashmap.num_elements = 0;
		new_hashmap.buffer = buffer;
		new_hashmap.capacity = (size_t)((float)(buffer_size - 1) / (sizeof(TKey) + sizeof(TValue) + sizeof(uint32_t) + 1/4.0));

		// clear the flags of the new hashmap
		uint8_t *new_flags_ptr = (uint8_t *) buffer;
		for (size_t i = 0; i < (new_hashmap.capacity / 4 + 1); i++) {
			new_flags_ptr[i] = 0;
		}

		// now re-insert the entries
		for (size_t pos = 0; pos < capacity; pos++) {
			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				new_hashmap.set(hashes_ptr[pos], keys_ptr[pos], values_ptr[pos]);
			}
		}

		return new_hashmap;

	}
};

}

#endif
