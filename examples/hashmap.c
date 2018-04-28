/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdio.h>
#include <string.h>
#include <assert.h>

#define CF_HASHMAP_IMPLEMENTATION
#include "cf_hashmap.h"


// uint32_t key and uint16_t value

static bool uint32_cmp(const void * restrict a, const void * restrict b)
{
	return *(const uint32_t * restrict)a == *(const uint32_t * restrict)b;
}

static void uint32_copy(const void * restrict from, void * restrict to)
{
	*(uint32_t * restrict)to = *(const uint32_t * restrict)from;
}

static void uint16_copy(const void * restrict from, void * restrict to)
{
	*(uint16_t *)to = *(const uint16_t * restrict)from;
}


// const char * key

static bool char_ptr_cmp(const void * restrict a, const void * restrict b)
{
	const char * restrict *_a = (const char * restrict *) a;
	const char * restrict *_b = (const char * restrict *) b;

	return *_a == *_b;
}

static void char_ptr_copy(const void * restrict from, void * restrict to)
{
	const char * restrict * _from = (const char * restrict *) from;
	char * restrict *_to = (char * restrict *) to;

	*_to = (void * restrict) *_from;
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

			printf("map[13] == %hu\n", value);
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

	{
		uint8_t buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(const char *, uint16_t, 3)];

		cf_hashmap map;

		{
			cf_hashmap_key_info key_info = {
			        sizeof(const char *),
			        &char_ptr_cmp,
			        &char_ptr_copy,
			};

			cf_hashmap_value_info value_info = {
			        sizeof(uint16_t),
			        &uint16_copy,
			};

			map = cf_hashmap_new(key_info,
			                     value_info,
			                     sizeof(buffer),
			                     buffer);
		}

#define CHAR_HASH(str) ((uint32_t) ((uintptr_t) str & 0xFFFFFFFF)) // don't judge me
		{
			// fill up hashtable
			const char *key = "Alice";
			uint16_t value = 23;

			cf_hashmap_set(&map, CHAR_HASH(key), &key, &value);

			key = "Bob";
			value = 31;
			cf_hashmap_set(&map, CHAR_HASH(key), &key, &value);

			key = "Eve";
			value = 1337;
			cf_hashmap_set(&map, CHAR_HASH(key), &key, &value);
		}

		{
			// iterate over all names and print them

			const char *key;
			uint16_t value;

			cf_hashmap_iter iter = cf_hashmap_iter_start(&map);

			while (cf_hashmap_iter_next(&map, &iter, &key, &value)) {
				printf("map[\"%s\"] = %hu\n", key, value);
			}
		}

		{
			printf("resize test\n");

			printf("old loadfactor: %f\n", cf_hashmap_load_factor(&map));

			uint8_t tmp_buffer[CF_HASHMAP_DECENT_BUFFER_SIZE(const char *, uint16_t, 256)];

			cf_hashmap new_map = cf_hashmap_copy(&map, sizeof(tmp_buffer), tmp_buffer);
			printf("new loadfactor: %f\n", cf_hashmap_load_factor(&new_map));


			const char *key;
			uint16_t value;

			cf_hashmap_iter iter = cf_hashmap_iter_start(&new_map);

			while (cf_hashmap_iter_next(&new_map, &iter, &key, &value)) {
				printf("new_map[\"%s\"] = %hu\n", key, value);
			}

		}
	}

	return 0;
}
