/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

///
/// This is a single-header library that provides an implementation of a
/// memory pool / pool allocator. This memory pool can be used to manage
/// fixed-size allocations in a user-given buffer.
///
#ifndef CF_MEMORYPOOL_HPP
#define CF_MEMORYPOOL_HPP

#include <stddef.h>
#include <stdint.h>

/// In some cases (where the type of data to be allocated is small, usually)
/// it is better to use a smaller indexing type (default is uint32_t).
/// In such a case, this macro calculates the size of a buffer that can hold
/// n elements.
#define CF_MEMORYPOOL_BUFFER_SIZE_CUSTOM_IDX(type, idx, n) \
	((sizeof(type) > sizeof(idx) ? sizeof(type) : sizeof(idx)) * n)

/// This macro calculates the size of a buffer that can hold n elements of
/// type `type`.
#define CF_MEMORYPOOL_BUFFER_SIZE(type, n) \
	CF_MEMORYPOOL_BUFFER_SIZE_CUSTOM_IDX(type, uint32_t, n)

namespace cf {

/// A memory pool type that manages allocations of fixed size elements of
/// type `T` in a user-given buffer.
/// The allocator is implemented by re-using free elements to store the
/// index of the next free element. This index type is uint32_t by default,
/// but a custom index type can be used as well.
/// Each element stored in the buffer is a union of the actual data and
/// the index type. So for maximum space efficiency, the index type should
/// be smaller than the value type.
template <typename T, typename IndexType = uint32_t>
struct memorypool {
private:

	size_t m_num_elements;

	size_t m_capacity;

	IndexType next_free;

	typedef union { T value; IndexType next; } element_t;

	element_t *m_buffer;

public:

	/// This function constructs a new memory pool. The `buffer` is a chunk of memory
	/// of size `buffer_size` which the memory pool will "draw" from.
	static memorypool<T, IndexType> create(size_t buffer_size, void *buffer)
	{
		memorypool<T, IndexType> pool;
		pool.m_buffer = (element_t *) buffer;

		pool.m_num_elements = 0;
		pool.m_capacity = buffer_size / sizeof(element_t);

		// now set all the next pointers to the next element
		for (IndexType i = 0; i < pool.m_capacity; i++) {
			pool.m_buffer[i].next = (i + 1) % pool.m_capacity;
		}

		pool.next_free = 0;

		return pool;
	}

	/// Allocate a new element of type `T`.
	/// If not enough space is available, `nullptr` will be returned.
	T *allocate()
	{
		if (m_num_elements == m_capacity) {
			return nullptr;
		}

		IndexType new_next_free = m_buffer[next_free].next;
		IndexType old_next_free = next_free;
		m_buffer[next_free].next = m_buffer[new_next_free].next;

		m_num_elements++;
		next_free = new_next_free;

		return &m_buffer[old_next_free].value;
	}

	/// Free a previously used element.
	void free(T *element)
	{
		IndexType index = ((element_t *) element - m_buffer);

		IndexType old_next_free = next_free;
		next_free = index;
		m_num_elements--;

		m_buffer[index].next = old_next_free;
	}

	/// The load factor of the memory pool (from 0.0 to 1.0).
	float load_factor() const
	{
		return (float)m_num_elements / m_capacity;
	}

	/// number of used elements in the memory pool
	size_t num_elements() const
	{
		return m_num_elements;
	}

	/// maxium number of elements the memory pool can hold.
	size_t capacity() const
	{
		return m_capacity;
	}

};

}


#endif
