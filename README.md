# cfstructs - cache friendly data structures

A collection of single-header C data structures with focus on cache friendliness and performance.

------

This project was mostly inspired by [Godot Engine](https://godotengine.org/)'s [`OAHashMap`](https://github.com/godotengine/godot/blob/b22f048700105dec26154cc90f10b0ef34b3f5ed/core/oa_hash_map.h) type.

Unfortunately, that class is implemented in C++, also it depends on other engine-internal classes, so it can't be ported easily to other projects.

Another huge inspiration is Google's [`dense_hash_map` type](https://github.com/sparsehash/sparsehash/blob/4cb924025b8c622d1a1e11f4c1e9db15410c75fb/src/sparsehash/dense_hash_map), which is also implemented in C++ but a lot easier to integrate into existing projects.

This repositoy aims to implement similar containers, focussed on performance and ease of use (or more like, ease of *integration*). All data structures implemented here should not depend on any third party libraries (including `libc`). Not handling memory **at all** leaves the user of the library more fine grained control over memory management.

## Container types

So far, only `cf_hashmap` is implemented.

### [`cf_hashmap`](https://github.com/karroffel/cfstructs/blob/master/cf_hashmap.h)

The `cf_hashmap` is a HashMap implementation that uses open addressing with linear probing. Its main focus is on lookup and iteration speed.

The user has to hand the `cf_hashmap` a buffer which will hold all the data managed by the hashmap. This buffer is a single chunk of memory, the hashmap however will internally divide this buffer into 4 different "regions":
 - flags
 - hashes
 - keys
 - values

The "flags" region stores two bits of information for each key-value pair: `is_filled` and `was_deleted`. The flags for 4 entries can be saved in one octet.

The "hashes" region stores the hash of each entry. Each hash is represented by a `uint32_t`.

The "keys" region stores the keys of each entry, which are needed to resolve possible hash collisions.

The "values" regions stores the values. Apart from storing and letting the caller read the value, the hashmap doesn't interact with this region much at all.

A simple usage example can be found in the [`examples/hashmap.c`](https://github.com/karroffel/cfstructs/blob/master/examples/hashmap.c) file.


## Planned

 - `cf_hashmap`: add a way to (maybe even incrementally) rehash the table onto a new, bigger table.
 - `cf_hashset`: a hashset that operates similarly to `cf_hashmap`, but doesn't provide key-value mapping, only existance checks.
 - `cf_poolalloc`: a pool allocator that manages allocations on a user-provided fixed-size buffer.
 - I don't know, maybe something else that I need in a project. 
