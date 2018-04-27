/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CF_HASHMAP_IMPLEMENTATION
#include "cf_hashmap.h"

bool uint32_cmp(const void * restrict a, const void * restrict b)
{
	return *(const uint32_t * restrict)a == *(const uint32_t * restrict)b;
}

void uint32_copy(const void * restrict from, void * restrict to)
{
	*(uint32_t * restrict)to = *(const uint32_t * restrict)from;
}

void uint16_copy(const void * restrict from, void * restrict to)
{
	*(uint16_t *)to = *(const uint16_t * restrict)from;
}

int main(int argc, char **argv)
{
	{
		uint8_t buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(uint32_t, uint16_t, 1024)];

		cf_hashmap map;

		{
			cf_hashmap_key_info key_info = {
			        sizeof(uint32_t),
			        &uint32_cmp,
			        &uint32_copy,
			};

			cf_hashmap_value_info value_info = {
			        sizeof(uint16_t),
			        &uint16_copy,
			};

			map = cf_hashmap_new(key_info,
			                     value_info,
			                     sizeof(buffer),
			                     buffer);

			printf("capacity: %zu\n", map.capacity);
		}

		{
			uint32_t key = 13;
			uint16_t value = 37;
			cf_hashmap_set(&map, key, &key, &value);

			if (!cf_hashmap_lookup(&map, key, &key, &value)) {
				printf("Whaaat, this is a bug. Please report it. Or better, fix it. =)\n");
			}

			value = 42;

			cf_hashmap_set(&map, key, &key, &value);

			cf_hashmap_lookup(&map, key, &key, &value);

			printf("map[13] == %u\n", value);
		}

		{
			// test removing
			uint32_t key = 42;
			uint32_t value = 21;

			cf_hashmap_set(&map, key, &key, &value);

			if (!cf_hashmap_lookup(&map, key, &key, &value)) {
				printf("Whaaat, this is a bug. Please report it. Or better, fix it. =)\n");
			}

			size_t num_elems = map.num_elements;

			cf_hashmap_remove(&map, key, &key);

			if (cf_hashmap_lookup(&map, key, &key, &value)) {
				printf("Whaaat, this is a bug. Please report it. Or better, fix it. =)\n");
			} else {
				printf("removing works! elems before: %zu elems after: %zu\n", num_elems, map.num_elements);
			}

		}
	}

	return 0;
}
