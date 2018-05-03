/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <assert.h>

#include "cf_hashset.hpp"

int main(int argc, char **argv)
{
	using cf::hashset;

	printf("=== cf_hashset tests ===\n");

	{
		uint8_t buffer[CF_HASHSET_GET_BUFFER_SIZE(uint32_t, 3)];

		auto set = hashset<uint32_t>::create(sizeof(buffer), buffer);

		assert(set.num_elements() == 0);

		set.insert(13, 13);

		assert(set.has(13, 13));
		assert(set.num_elements() == 1);

		set.insert(13, 13); // insert the same thing again
		assert(set.num_elements() == 1);

		set.insert(1337, 1337);
		assert(set.num_elements() == 2);

		set.remove(13, 13);
		assert(!set.has(13, 13));
		assert(set.num_elements() == 1);

		set.insert(13, 13);
		set.insert(13, 21); // cause a hash collision

		{
			// iter test
			printf("=== iterator test ===\n");

			auto iter = set.iter_start();

			uint32_t value;

			printf("{");
			while (set.iter_next(iter, value)) {
				printf("%u, ", value);
			}
			printf("}\n");
		}

		{
			// copy test
			printf("=== copy test ===\n");

			printf("old loadfactor: %f\n", set.load_factor());

			uint8_t new_buffer[CF_HASHSET_GET_BUFFER_SIZE(uint32_t, 256)];
			auto new_set = set.copy(sizeof(new_buffer), new_buffer);

			printf("new loadfactor: %f\n", new_set.load_factor());


			auto iter = new_set.iter_start();

			uint32_t value;

			printf("{");
			while (new_set.iter_next(iter, value)) {
				printf("%u, ", value);
			}
			printf("}\n");
		}
	}
	return 0;
}
