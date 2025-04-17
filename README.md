# vec_list

A header-only replacement for `std::list` with better performance characteristics.

## Installation and requirements

Copy `header/vec_list.h` and include it. Requires C++ 20. 

## Description

`vec_list` uses geometric allocation like `std::vector` and keeps a list of holes when elements are deleted. When it becomes full, it allocates another block and keeps the previous ones. This is required so iterators don't get invalidated.

This improves over `std::list` which uses a `new`/`delete` on every insertion/erasure, while still keeping other traits like (amortized) constant time insertion and persistent iterators.

### API support

`vec_list` supports the entire `std::list` api with the exception of the "algorithm" functions:
* `merge()`
* `sort()`
* `unique()`
* `remove_if()`
* `erase_if()`

`splice()` is supported and optimized the same way as `std::list` for full lists, but not partial lists since this is not possible.

### Extensions

`vec_list` provides some additional functions over `std::list`:
* `reserve(size_t n)` allocates at least enough memory to fit `n` elements before needing another allocation.
* `capacity()` returns how many elements can fit in the list before needing another allocation.
* `optimize(bool shrink_to_fit)` makes the elements as contiguous as possible. This is the only function which invalidates iterators. If `shrink_to_fit` is `true`, it will also free as much unused memory as possible.