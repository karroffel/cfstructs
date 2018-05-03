/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides a cache-friendly hash map
/// implementation that uses open addressing with robin hood hashing.
///
#ifndef CF_HASHMAP_HPP
#define CF_HASHMAP_HPP

#include <stdint.h>
#include <stddef.h>

namespace cf {

#define CF_HASHMAP_GET_BUFFER_SIZE(key_type, value_type, num_elements) \
	((sizeof(uint32_t) + sizeof(key_type) + sizeof(value_type)) * num_elements)

/// A hashmap type that uses open addressing with robinhood hashing.
/// The hashmap uses 3 different regions of memory: hashes, keys and values.
/// Those regions are located next to each other in a buffer in a Struct-of-Arrays
/// kind of fashion.
/// This hashmap doesn't perform any hashing itself, so the the user has to provide
/// the hash values. The keys have to be POD types. Comparision of keys to
/// resolve hash collisions uses operator==, so this might need to be implemented
/// if the key type is not a primitive type.
template <typename TKey, typename TValue>
struct hashmap {

private:
	size_t m_num_elements;

	size_t m_capacity;

	uint8_t *m_buffer;

	static const uint32_t EMPTY_HASH = 0;
	static const uint32_t DELETED_HASH_BIT = 1 << 31;

	template <typename T>
	static void _swap(T &a, T &b) {
		T tmp = a;
		a = b;
		b = tmp;
	}

	// We want only hashes != 0 since we use that for detecting empty
	// entries.
	// The leftmost bit indicates that the entry was deleted in the past.
	// We can't use just a "deleted value" because otherwise we lose information
	// about the previous probe distance
	static uint32_t _hash(uint32_t hash) {
		if (hash == EMPTY_HASH) {
			hash = EMPTY_HASH + 1;
		} else if (hash & DELETED_HASH_BIT) {
			hash &= ~DELETED_HASH_BIT;
		}

		return hash;
	}

	uint32_t _get_probe_distance(uint32_t pos, uint32_t hash) const {
		hash = hash & ~DELETED_HASH_BIT;

		uint32_t ideal_pos = hash % m_capacity;

		return pos - ideal_pos;
	}

	bool _lookup_pos(uint32_t hash, const TKey &key, uint32_t &pos) const {
		pos = hash % m_capacity;
		uint32_t distance = 0;

		uint32_t *hashes = (uint32_t *) m_buffer;
		TKey *keys = (TKey *) (m_buffer + m_capacity * sizeof(uint32_t));

		while (42) {
			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_distance(pos, hashes[pos])) {
				return false;
			}

			if (hashes[pos] == hash && keys[pos] == key) {
				return true;
			}

			pos = (pos + 1) % m_capacity;
			distance++;
		}
	}

	void insert(uint32_t hash, const TKey &key, const TValue &value) {

		uint32_t distance = 0;
		uint32_t pos = hash % m_capacity;

		TKey _key = key;
		TValue _value = value;

		uint32_t *hashes = (uint32_t *) m_buffer;
		TKey *keys = (TKey *) (m_buffer + sizeof(uint32_t) * m_capacity);
		TValue *values = (TValue *) (m_buffer + (sizeof(uint32_t) + sizeof(TKey)) * m_capacity);

		while (42) {

			// An empty slot, put our stuff in there, then we're done!
			if (hashes[pos] == EMPTY_HASH) {
				hashes[pos] = hash;
				keys[pos] = _key;
				values[pos] = _value;
				m_num_elements++;
				return;
			}

			uint32_t exiting_distance = _get_probe_distance(pos, hashes[pos]);
			if (exiting_distance < distance) {
				// we found a slot that should be further to the right

				if (hashes[pos] & DELETED_HASH_BIT) {
					// buuuut it was deleted so we can use it

					hashes[pos] = hash;
					keys[pos] = _key;
					values[pos] = _value;
					m_num_elements++;
					return;
				}

				// swap out the entry and now operate on the other value
				// that should be further to the right
				_swap(hash, hashes[pos]);
				_swap(_key, keys[pos]);
				_swap(_value, values[pos]);
				distance = exiting_distance;
			}

			pos = (pos + 1) % m_capacity;
			distance++;
		}

	}

public:

	/// An iterator for iterating over key-value pairs. Use `iter_start()` to acquire
	/// such an iterator. Use `iter_next()` to advance the iteration.
	struct iter {
		size_t offset;
	};

	/// This function constructs a new hashmap value.
	/// The buffer is a chunk of memory that will be used as the storage. It should probably be
	/// created by using the `CF_HASHMAP_GET_BUFFER_SIZE` macro.
	/// Don't modify the contents of the buffer after handing it to a hashmap.
	static hashmap create(size_t buffer_size, void *buffer)
	{
		hashmap<TKey, TValue> map = {};

		map.m_buffer = (uint8_t *) buffer;
		map.m_num_elements = 0;
		map.m_capacity = (size_t)(buffer_size / (sizeof(TKey) + sizeof(TValue) + sizeof(uint32_t)));

		size_t capacity = map.m_capacity;

		uint32_t *hashes = (uint32_t *) map.m_buffer;

		for (size_t i = 0; i < capacity; i++) {
			hashes[i] = EMPTY_HASH;
		}

		return map;
	}

	/// Associate a key with a value. The hash is the hash value of the key. This hashmap doesn't
	/// perform any hashing itself, so the caller has to provide the hash.
	/// For collision resolution, the key itself has to be provided as well.
	void set(uint32_t hash, const TKey &key, const TValue &value)
	{
		hash = _hash(hash);
		uint32_t pos = 0;
		bool exists = _lookup_pos(hash, key, pos);

		TValue *values = (TValue *) (m_buffer + (sizeof(uint32_t) + sizeof(TKey)) * m_capacity);

		if (exists) {
			values[pos] = value;
		} else {
			insert(hash, key, value);
		}
	}

	/// Lookup a value in the hashmap. The hash is the hash of the key. The key value itself
	/// has to be provided in case a collision occurs.
	/// If an entry is found, the value will be written to the out-parameter `value`.
	/// Returns true if an entry was found, false otherwise.
	bool lookup(uint32_t hash, const TKey &key, TValue &value) const
	{
		uint32_t pos = 0;
		bool exists = _lookup_pos(_hash(hash), key, pos);

		TValue *values = (TValue *) (m_buffer + (sizeof(uint32_t) + sizeof(TKey)) * m_capacity);

		if (exists) {
			value = values[pos];
			return true;
		}

		return false;
	}

	/// Get the value associated with the given hash and key.
	/// This uses `lookup()` internally, but discards the bool return value
	/// and returns the value instead of having it as an out parameter.
	/// WARNING: If the entry was not found then the return value is **undefined**.
	inline TValue get(uint32_t hash, const TKey &key) const
	{
		TValue value;
		lookup(_hash(hash), key, value);
		return value;
	}

	/// Remove an entry from the hashtable by proving the hash and the key value, in case a
	/// collision occurs.
	void remove(uint32_t hash, const TKey &key)
	{
		uint32_t pos = 0;
		bool exists = _lookup_pos(_hash(hash), key, pos);
		uint32_t *hashes = (uint32_t *) m_buffer;

		if (!exists) {
			return;
		}

		hashes[pos] |= DELETED_HASH_BIT;
		m_num_elements--;
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
		uint32_t *hashes = (uint32_t *) m_buffer;
		TKey *keys = (TKey *) (m_buffer + sizeof(uint32_t) * m_capacity);
		TValue *values = (TValue *) (m_buffer + (sizeof(uint32_t) + sizeof(TKey)) * m_capacity);

		for (size_t i = iter.offset; i < m_capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}
			if (hashes[i] & DELETED_HASH_BIT) {
				continue;
			}

			key = keys[i];
			value = values[i];
			iter.offset = i + 1;
			return true;
		}
		return false;
	}

	/// Calculates the load factor of the map. When the load factor is greater than 0.95
	/// then `copy()` should be used to relocate the hashmap for better performance.
	inline float load_factor() const
	{
		return m_num_elements / (float) m_capacity;
	}

	/// Creates a new hashmap using a different buffer. All the entries of the
	/// current map will be inserted into the new map.
	hashmap<TKey, TValue> copy(size_t buffer_size, void *buffer) const
	{
		hashmap<TKey, TValue> new_hashmap = hashmap::create(buffer_size, buffer);

		uint32_t *hashes = (uint32_t *) m_buffer;
		TKey *keys = (TKey *) (m_buffer + sizeof(uint32_t) * m_capacity);
		TValue *values = (TValue *) (m_buffer + (sizeof(uint32_t) + sizeof(TKey)) * m_capacity);

		for (size_t i = 0; i < m_capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}
			if (hashes[i] & DELETED_HASH_BIT) {
				continue;
			}

			new_hashmap.insert(hashes[i], keys[i], values[i]);
		}

		return new_hashmap;

	}

	/// The number of elements in this hash map.
	size_t num_elements() const
	{
		return m_num_elements;
	}

	/// The capacity of how many elements *could* be held in this map.
	size_t capacity() const
	{
		return m_capacity;
	}
};

}

#endif
