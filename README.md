# vec_list

A header-only drop-in replacement for `std::list` with better performance characteristics.

## Installation and requirements

Copy `header/vec_list.h` and include it. Requires C++ 20. 

## Description

`vec_list` uses geometric allocation like `std::vector` and keeps a list of holes when elements are deleted. When it becomes full, it allocates another block and keeps the previous ones. This is required so iterators don't get invalidated.

This improves over `std::list` which uses a `new`/`delete` on every insertion/erasure, while still keeping other traits like (amortized) constant time insertion and persistent iterators.

You can replace every `std::list` with `palla::vec_list` since they have the same api, unless you are using custom allocators as those are not supported.
