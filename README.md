# cfstructs - cache friendly data structures

A collection of single-header C++ data structures with focus on cache friendliness and performance.

------

This project was mostly inspired by [Godot Engine](https://godotengine.org/)'s [`OAHashMap`](https://github.com/godotengine/godot/blob/b22f048700105dec26154cc90f10b0ef34b3f5ed/core/oa_hash_map.h) type.

Unfortunately, that class depends on other engine-internal classes, so it can't be ported easily to other projects.

Another huge inspiration is Google's [`dense_hash_map` type](https://github.com/sparsehash/sparsehash/blob/4cb924025b8c622d1a1e11f4c1e9db15410c75fb/src/sparsehash/dense_hash_map), which is a lot easier to integrate into existing projects, but not as simple as a single-header library.

This repositoy aims to implement similar containers, focussed on performance and ease of use (or more like, ease of *integration*). All data structures implemented here should not depend on any third party libraries (including `libc`). Not handling memory **at all** leaves the user of the library more fine grained control over memory management.

## Container types

### [`cf::hashmap`](https://github.com/karroffel/cfstructs/blob/master/cf_hashmap.hpp)

The `cf::hashmap` is a HashMap implementation that uses open addressing with linear probing. Its main focus is on lookup and robinhood hashing.

The user has to hand the `cf::hashmap` a buffer which will hold all the data managed by the hashmap. This buffer is a single chunk of memory, the hashmap however will internally divide this buffer into 3 different "regions":
 - hashes
 - keys
 - values

The "hashes" region stores the hash of each entry. Each hash is represented by a `uint32_t`.

The "keys" region stores the keys of each entry, which are needed to resolve possible hash collisions.

The "values" regions stores the values. Apart from storing and letting the caller read the value, the hashmap doesn't interact with this region much at all.

A simple usage example can be found in the [`examples/hashmap.cpp`](https://github.com/karroffel/cfstructs/blob/master/examples/hashmap.cpp) file.

### [`cf::hashset`](https://github.com/karroffel/cfstructs/blob/master/cf_hashset.hpp)

The `cf::hashset` is a HashSet implementation that operates in a similar way to `cf::hashmap` in that it uses open addressing with linear probing.

As with the `cf::hashmap`, this container hold all the data in a user-provided buffer. That buffer gets internally divided into 3 "regions":
 - flags
 - hashes
 - values

The regions have the same purpose as with `cf::hashmap`, but the "values" region corresponds to the "keys" region in the HashMap.

A simple usage example can be found in the [`examples/hashset.cpp`](https://github.com/karroffel/cfstructs/blob/master/examples/hashset.cpp) file.

### [`cf::memorypool`](https://github.com/karroffel/cfstructs/blob/master/cf_memorypool.hpp)

A basic memory allocator that uses a user provided buffer to allocate fixed size elements.

The allocator works by re-using unused elements to point to the next unused element.
When a new element should be allocated, the allocator grabs the last unused element it knew about and sets the pointer to the next unused element to the one saved in the previously allocated element.

Because unused elements are mis-used to store the index to the next free element, only POD types can be used.
Furthermore, the space required for each element in the buffer is the size of **the union of element type and index type**.

For example, when using `uint8_t` as element type and `uint32_t` as index type, each element will need the same space as the `uint32_t`.
The index type can be customized but defaults to `uint32_t`.

A simple usage example can be found in the [`examples/memorypool.cpp`](https://github.com/karroffel/cfstructs/blob/master/examples/memorypool.cpp) file.

## Planned

 - I don't know, maybe something else that I need in a project. 
