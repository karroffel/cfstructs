/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides a chache-friend hash set that
/// uses open addressing with robinhood hashing.
///
#ifndef CF_HASHSET_HPP
#define CF_HASHSET_HPP

#include <stddef.h>
#include <stdint.h>

namespace cf {

#define CF_HASHSET_GET_BUFFER_SIZE(key_type, num_elements) \
	((sizeof(uint32_t) + sizeof(key_type)) * num_elements)

/// A hashset type that uses open addressing with robinhood hashing.
/// The hashset uses 2 different regions of memory: hashes and values.
/// Those regions are located next to each other in a buffer in a Struct-of-Arrays
/// kind of fashion.
/// The hashes are the hashes provided by the user. This hashset doesn't do any hashing itself.
/// The values region contains the values. They are used to resolve collisions and check for existance.
template <typename T>
struct hashset {

private:
	size_t m_num_elements;

	size_t m_capacity;

	uint8_t *m_buffer;

	static const uint32_t EMPTY_HASH = 0;
	static const uint32_t DELETED_HASH_BIT = 1 << 31;

	template <typename A>
	static void _swap(A &a, A &b) {
		A tmp = a;
		a = b;
		b = tmp;
	}

	// We want only hashes != 0 since we use that for detecting empty
	// entries.
	// The leftmost bit indicates that the entry was deleted in the past.
	// We can't use just a "deleted value" because otherwise we lose information
	// about the previous probe distance
	static uint32_t _hash(uint32_t hash)
	{
		if (hash == EMPTY_HASH) {
			hash = EMPTY_HASH + 1;
		} else if (hash & DELETED_HASH_BIT) {
			hash &= ~DELETED_HASH_BIT;
		}

		return hash;
	}

	uint32_t _get_probe_distance(uint32_t pos, uint32_t hash) const
	{
		hash = hash & ~DELETED_HASH_BIT;

		uint32_t ideal_pos = hash % m_capacity;

		return pos - ideal_pos;
	}

	bool _lookup_pos(uint32_t hash, const T &value, uint32_t &pos) const
	{
		pos = hash % m_capacity;
		uint32_t distance = 0;

		uint32_t *hashes = (uint32_t *) m_buffer;
		T *values = (T *) (m_buffer + m_capacity * sizeof(uint32_t));

		while (distance < m_capacity) {
			if (hashes[pos] == EMPTY_HASH) {
				return false;
			}

			if (distance > _get_probe_distance(pos, hashes[pos])) {
				return false;
			}

			if (hashes[pos] == hash && values[pos] == value) {
				return true;
			}

			pos = (pos + 1) % m_capacity;
			distance++;
		}

		return false;
	}

	void _insert(uint32_t hash, const T &value)
	{
		if (m_num_elements == m_capacity) {
			// if this is the case then this will just keep trying to
			// swap stuff around, never terminating.
			return;
		}
		uint32_t distance = 0;
		uint32_t pos = hash % m_capacity;

		T _value = value;

		uint32_t *hashes = (uint32_t *) m_buffer;
		T *values = (T *) (m_buffer + sizeof(uint32_t) * m_capacity);

		while (distance < m_capacity) {

			// An empty slot, put our stuff in there, then we're done!
			if (hashes[pos] == EMPTY_HASH) {
				hashes[pos] = hash;
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
					values[pos] = _value;
					m_num_elements++;
					return;
				}

				// swap out the entry and now operate on the other value
				// that should be further to the right
				_swap(hash, hashes[pos]);
				_swap(_value, values[pos]);
				distance = exiting_distance;
			}

			pos = (pos + 1) % m_capacity;
			distance++;
		}
	}
public:
	/// An iterator for iterating over the values. Use `iter_start()` to acquire
	/// such an iterator. Use `iter_next()` to advance the iteration.
	struct iter {
		size_t offset;
	};

	/// This functions constructs a new hashet value.
	/// The buffer is a chunk of memory that will be used as the storage. That
	/// buffer should probably be created/sized by using the `CF_HASHSET_GET_BUFFER_SIZE` macro.
	/// Don't modify the contents of the buffer after handing it to a hashset.
	static hashset create(size_t buffer_size, void *buffer)
	{
		hashset<T> set = {};

		set.m_buffer = (uint8_t *) buffer;
		set.m_num_elements = 0;
		set.m_capacity = (size_t)(buffer_size / (sizeof(T) + sizeof(uint32_t)));

		size_t capacity = set.m_capacity;
		uint32_t *hashes = (uint32_t *) set.m_buffer;

		// Set the flags os that every element is considered empty
		for (size_t i = 0; i < capacity; i++) {
			hashes[i] = EMPTY_HASH;
		}

		return set;
	}

	/// Inserts a value into the hashset. Since this hashset doesn't perform any hashing
	/// itself, the caller has to provide the hash value.
	/// The value is used for checking for existance as well as collision resolution.
	void insert(uint32_t hash, const T &value)
	{
		hash = _hash(hash);
		uint32_t pos = 0;
		bool exists = _lookup_pos(hash, value, pos);

		if (exists) {
			return;
		}

		_insert(hash, value);
	}

	/// Checks if `value` is an element of the hashset.
	/// Returns true if an entry with `value` was found, false otherwise.
	bool has(uint32_t hash, const T &value) const
	{
		hash = _hash(hash);
		uint32_t _pos = 0;
		return _lookup_pos(hash, value, _pos);
	}

	/// Remove a value from the hashset.
	void remove(uint32_t hash, const T &value)
	{
		hash = _hash(hash);
		uint32_t pos = 0;
		bool exists = _lookup_pos(hash, value, pos);

		if (!exists) {
			return;
		}

		uint32_t *hashes = (uint32_t *) m_buffer;
		hashes[pos] |= DELETED_HASH_BIT;
		m_num_elements--;
	}

	/// Creates an iterator for the hashset. Use `iter_next()` to advance the iteration.
	iter iter_start() const
	{
		hashset<T>::iter iter = {};
		iter.offset = 0;
		return iter;
	}

	/// Advance the iterator to the next element found. The value will be written to the
	/// out parameter `value` if an element was found.
	/// If an element was found, true will be returned, otherwise false.
	bool iter_next(iter &iter, T &value) const
	{
		uint32_t *hashes = (uint32_t *) m_buffer;
		T *values = (T *) (m_buffer + sizeof(uint32_t) * m_capacity);

		for (size_t i = iter.offset; i < m_capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}
			if (hashes[i] & DELETED_HASH_BIT) {
				continue;
			}

			value = values[i];
			iter.offset = i + 1;
			return true;
		}
		return false;
	}

	/// Calculates the load factor of the hashset. If the load factor is greater than 0.95
	/// then `copy()` should be used to relocate the hashet for better performance.
	inline float load_factor() const
	{
		return m_num_elements / (float) m_capacity;
	}

	/// Creates a new hashset using a different buffer. All values of the current map
	/// will be inserted into the new map.
	hashset<T> copy(size_t buffer_size, void *buffer) const
	{
		hashset<T> new_hashset = hashset::create(buffer_size, buffer);

		uint32_t *hashes = (uint32_t *) m_buffer;
		T *values = (T *) (m_buffer + sizeof(uint32_t) * m_capacity);

		for (size_t i = 0; i < m_capacity; i++) {
			if (hashes[i] == EMPTY_HASH) {
				continue;
			}
			if (hashes[i] & DELETED_HASH_BIT) {
				continue;
			}

			new_hashset.insert(hashes[i], values[i]);
		}

		return new_hashset;
	}

	/// The number of elements in this hashset.
	size_t num_elements() const
	{
		return m_num_elements;
	}

	/// The capacity of how many elements *could* be held in this set.
	size_t capacity() const
	{
		return m_capacity;
	}

};


}

#endif
