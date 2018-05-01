/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides a chache-friend hash set that
/// uses open addressing with local probing. This hashset doesn't perform any hashing
/// itself, so the the user has to provide the hash values. The keys have to be POD
/// types. Comparision of values to resolving hash collisions uses operator==, so
/// this might need to be implemented if the value type is not a primitive type.
///
#ifndef CF_HASHSET_HPP
#define CF_HASHSET_HPP

#include <stddef.h>
#include <stdint.h>

namespace cf {

#define _CF_HASHSET_DECENT_BUFFER_SIZE(key_size, capacity) \
	((sizeof(uint32_t) + key_size) * capacity + (capacity / 4) + 1)

/// Calculates the size in bytes for a buffer for a hashset with estimated `num_elements`
/// entries. The calculated size gives enough space to not go over 75% load factor.
/// Wikipedia says that 80% are a nice spot before the local probing takes too much time.
/// https://en.wikipedia.org/wiki/Open_addressing
#define CF_HASHSET_DECENT_BUFFER_SIZE(key_type, num_elements) \
	_CF_HASHSET_DECENT_BUFFER_SIZE(sizeof(key_type), (int)(1.5 * num_elements))

/// A hashset type that uses open addressing with local probing.
/// The hashset uses 3 different regions of memory: flags, hashes and values.
/// Those regions are located next to each other in a buffer in a Struct-of-Arrays
/// kind of fashion.
/// The flags store information about wheter the entry is filled or was filled in the past.
/// The hashes are the hashes provided by the user. This hashset doesn't do any hashing itself.
/// The values region contains the values. They are used to resolve collisions and check for existance.
template <typename T>
struct hashset {

private:
	size_t m_num_elements;

	size_t m_capacity;

	void *m_buffer;

public:
	/// An iterator for iterating over the values. Use `iter_start()` to acquire
	/// such an iterator. Use `iter_next()` to advance the iteration.
	struct iter {
		size_t offset;
	};

	/// This functions constructs a new hashet value.
	/// The buffer is a chunk of memory that will be used as the storage. That
	/// buffer should probably be created/sized by using the `CF_HASHSET_DECENT_BUFFER_SIZE` macro.
	/// Don't modify the contents of the buffer after handing it to a hashset.
	static hashset create(size_t buffer_size, void *buffer)
	{
		hashset<T> set = {};

		set.m_buffer = buffer;
		set.m_num_elements = 0;
		set.m_capacity = (size_t)((float)(buffer_size - 1) / (sizeof(T) + sizeof(uint32_t) + 1/4.0));

		size_t capacity = set.m_capacity;
		uint8_t *flags_ptr = (uint8_t *) set.m_buffer;

		// Set the flags os that every element is considered empty
		for (size_t i = 0; i < (capacity / 4 + 1); i++) {
			flags_ptr[i] = 0;
		}

		return set;
	}

	/// Inserts a value into the hashset. Since this hashset doesn't perform any hashing
	/// itself, the caller has to provide the hash value.
	/// The value is used for checking for existance as well as collision resolution.
	void insert(uint32_t hash, const T &value)
	{
		const size_t capacity = this->m_capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->m_buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		T *values_ptr = (T *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && values_ptr[pos] == value) {
					// entry already exists! Just don't do anything then.
					return;
				}
				continue;
			}

			hashes_ptr[pos] = hash;
			values_ptr[pos] = value;

			// set the filled flag
			flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset));
			// also unset the "deleted" flag... you never know!
			flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset + 1));

			m_num_elements++;
			return;
		}
	}

	/// Checks if `value` is an element of the hashset.
	/// Returns true if an entry with `value` was found, false otherwise.
	bool has(uint32_t hash, const T &value) const
	{
		const size_t capacity = this->m_capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->m_buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		T *values_ptr = (T *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
			bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && values_ptr[pos] == value) {
					// found the entry! Return true
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

	/// Remove a value from the hashset.
	void remove(uint32_t hash, const T &value)
	{
		const size_t capacity = this->m_capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->m_buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		T *values_ptr = (T *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		for (size_t i = 0; i < capacity; i++) {
			size_t pos = (hash + i) % capacity;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));
			bool was_deleted = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset + 1));

			if (is_filled) {
				if (hashes_ptr[pos] == hash && values_ptr[pos] == value) {
					// ayyyy, this is the entry, which means we get to kick it out now >:D
					flags_ptr[flag_pos] &= ~(1 << (2 * flag_pos_offset));
					flags_ptr[flag_pos] |= (1 << (2 * flag_pos_offset + 1));

					m_num_elements--;
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
		const size_t capacity = this->m_capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->m_buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		T *values_ptr = (T *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		for (size_t i = iter.offset; i < capacity; i++) {
			size_t pos = i;

			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				// heyyy, we found something, let's write the offset to the iterator and
				// copy the value!

				iter.offset = i + 1; // next time check the next entry

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

	/// Calculates the load factor of the hashset. If the load factor is greater than 0.8
	/// then `copy()` should be used to relocate the hashet for better performance.
	inline float load_factor() const
	{
		return m_num_elements / (float) m_capacity;
	}

	/// Creates a new hashset using a different buffer. All values of the current map
	/// will be inserted into the new map.
	hashset<T> copy(size_t buffer_size, void *buffer) const
	{
		const size_t capacity = this->m_capacity; // To assure to the compiler that it's constant
		uint8_t *flags_ptr = (uint8_t *) this->m_buffer;
		uint32_t *hashes_ptr = (uint32_t *) (flags_ptr + (capacity / 4 + 1));
		T *values_ptr = (T *) ((uint8_t *) hashes_ptr + sizeof(uint32_t) * capacity);

		// create the new hashset
		hashset<T> new_hashset;
		new_hashset.m_num_elements = 0;
		new_hashset.m_buffer = buffer;
		new_hashset.m_capacity = (size_t)((float)(buffer_size - 1) / (sizeof(T) + sizeof(uint32_t) + 1/4.0));

		// clear the flags of the new hashset
		uint8_t *new_flags_ptr = (uint8_t *) buffer;
		for (size_t i = 0; i < (new_hashset.m_capacity / 4 + 1); i++) {
			new_flags_ptr[i] = 0;
		}

		// now re-insert the values
		for (size_t pos = 0; pos < capacity; pos++) {
			size_t flag_pos = pos / 4;
			size_t flag_pos_offset = pos % 4;

			bool is_filled = flags_ptr[flag_pos] & (1 << (2 * flag_pos_offset));

			if (is_filled) {
				new_hashset.insert(hashes_ptr[pos], values_ptr[pos]);
			}
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
