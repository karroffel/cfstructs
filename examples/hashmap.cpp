/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#include "cf_hashmap.hpp"

int main(int argc, char **argv)
{
	using cf::hashmap;

	printf("=== cf_hashmap tests ===\n");

	{
		uint8_t buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(uint32_t, uint16_t, 1024)];
		auto map = hashmap<uint32_t, uint16_t>::create(sizeof(buffer), buffer);
		assert(map.num_elements() == 0);

		map.set(13, 13, 42);
		assert(map.num_elements() == 1);

		map.set(13, 13, 37);
		assert(map.num_elements() == 1);

		{
			uint16_t value;

			if (map.lookup(13, 13, value)) {
				printf("map[13] = %hu\n", value);
			} else {
				printf("Whaaat, this is a bug. Please report it. Or better, fix it. =)\n");
			}
		}

		map.set(13, 42, 1337); // cause a hash collision
		printf("map[42] = %hu\n", map.get(13, 42));

		{
			uint16_t value;
			map.remove(13, 42);
			assert(map.num_elements() == 1);
			assert(!map.lookup(13, 42, value));
		}

		map.set(12, 12, 24);
		map.set(1337, 1337, 7331);

		{
			// iter test
			printf("=== iterator test ===\n");
			auto iter = map.iter_start();

			uint32_t key;
			uint16_t value;

			while (map.iter_next(iter, key, value)) {
				printf("map[%u] = %hu\n", key, value);
			}
		}

	}

#define CHAR_HASH(str) ((uint32_t) ((uintptr_t) str & 0xFFFFFFFF)) // don't judge me
	{
		// This actually performs pointer comparison. Just so you know.
		// If you want to have value comparisons then a POD string type
		// (probably just a wrapper for `const char *` should be used.

		printf("=== const char * test ===\n");

		uint8_t old_buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(const char *, uint16_t, 3)];
		auto map = hashmap<const char *, uint16_t>::create(sizeof(old_buffer), old_buffer);

		map.set(CHAR_HASH("Alice"), "Alice", 23);
		map.set(CHAR_HASH("Bob"), "Bob", 31);
		map.set(CHAR_HASH("Eve"), "Eve", 1337);

		{
			// iteration test
			auto iter = map.iter_start();

			const char *key;
			uint16_t value;

			while (map.iter_next(iter, key, value)) {
				printf("map[\"%s\"] = %hu\n", key, value);
			}
		}

		{
			printf("=== resize test ===\n");

			printf("old loadfactor: %f\n", map.load_factor());

			uint8_t new_buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(const char *, uint16_t, 256)];
			auto new_map = map.copy(sizeof(new_buffer), new_buffer);

			printf("new loadfactor: %f\n", new_map.load_factor());


			auto iter = new_map.iter_start();

			const char *key;
			uint16_t value;

			while (new_map.iter_next(iter, key, value)) {
				printf("map[\"%s\"] = %hu\n", key, value);
			}
		}
	}

	return 0;
}
