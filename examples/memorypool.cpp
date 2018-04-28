/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <assert.h>

#include "cf_memorypool.hpp"

struct Velocity {
	float x;
	float y;
};

int main(int argc, char **argv)
{
	printf("=== memory pool tests ===\n");

	{
		uint8_t buffer[CF_MEMORYPOOL_BUFFER_SIZE(Velocity, 5)];
		auto pool = cf::memorypool<Velocity>::create(sizeof(buffer), buffer);

		{
			Velocity *ptrs[5];

			for (int i = 0; i < 5; i++) {
				ptrs[i] = pool.allocate();
			}

			pool.free(ptrs[1]);
			pool.free(ptrs[3]);

			ptrs[1] = pool.allocate();
			ptrs[3] = pool.allocate();

			// make sure non of them are null
			for (int i = 0; i < 5; i++) {
				printf("ptrs[%d] = %p\n", i, ptrs[i]);
			}

			pool.free(ptrs[0]);
			pool.free(ptrs[2]);
			pool.free(ptrs[4]);

			printf("load factor: %f\n", pool.load_factor());

			pool.free(ptrs[1]);
			pool.free(ptrs[3]);
		}

		for (int i = 0; i < 5; i++) {
			pool.allocate(); // allocate and leak. Just fill the pool
		}

		assert(pool.allocate() == nullptr);

	}
	return 0;
}
