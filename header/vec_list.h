#pragma once

#include <vector>
#include <optional>
#include <algorithm>
#include <cassert>

namespace palla {
    namespace details {
        namespace vec_list_namespace {


            // An std::list living inside a vector.
            // Allocates geometrically to reduce new's while still keeping every trait of std::list.
            // Keeps another (singlely-linked) list of holes when elements are erased and fills them back up later.
            // There is probably a clever way to do this using custom allocators on std::list.
            template<class T>
            class vec_list {
            private:
                // Private types.

                // Struct for elements.
                struct node {
                    node* next = nullptr;
                    node* prev = nullptr;
                    std::optional<T> elem;	// TODO optimize the optional by fudging the pointers in unused bits and directly storing a union of T and char.
                };

                // Iterators, templated for constness.
                template<class U>
                class iterator_impl {
                private:
                    // Private constructor so vec_list can create a valid iterator.
                    friend class vec_list;
                    explicit iterator_impl(node* node) : m_node(node) {}

                    // Private members.
                    node* m_node = nullptr;	// The iterator is basically a wrapper around a node.

                public:
                    // Types required to satisfy std::bidirectional_iterator.
                    using difference_type = std::ptrdiff_t;
                    using value_type = U;

                    // Default constructor. The user can only create empty iterators.
                    iterator_impl() = default;

                    // Indirection.
                    U& operator*() const { return *m_node->elem; }
                    U* operator->() const { return &**this; }

                    // Increment and decrement.
                    iterator_impl& operator++() { assert(m_node->next); *this = iterator(m_node->next); return *this; }
                    iterator_impl operator++(int) { iterator_impl current = *this; ++(*this); return current; }

                    iterator_impl& operator--() { assert(m_node->prev); *this = iterator(m_node->prev); return *this; }
                    iterator_impl operator--(int) { iterator_impl current = *this; --(*this); return current; }

                    // Comparison.
                    friend bool operator==(const iterator_impl& a, const iterator_impl& b) { return a.m_node == b.m_node; }

                    // Conversion from mutable to const.
                    template<class = std::enable_if_t<!std::is_const_v<U>>>
                    operator iterator_impl<const U>() const { return iterator_impl<const U>(m_node); }

                };


                // Private members.
                std::vector<std::vector<node>> m_buckets;	// List of buckets because they are never deleted. The first bucket is always 2 elements: begin and end.
                node* m_first_hole = nullptr;				// First hole. Holes form a singlely linked list. When an element is erased, it becomes the new first hole.
                size_t m_size = 0;							// Number of elements (not holes).

                // Expansion constants.
                static constexpr size_t MIN_BUCKET_SIZE = 4;
                static constexpr size_t GROWTH_FACTOR = 2;


                // Private functions.

                // Adds a bucket. Used when there is no more space and all holes are filled. 
                // m_first_hole is guaranteed to point to a hole after this.
                void add_bucket() {
                    m_buckets.emplace_back(m_buckets.size() == 1 ? MIN_BUCKET_SIZE : m_buckets.back().size() * GROWTH_FACTOR);
                    fill_bucket_with_holes(m_buckets.size() - 1);
                    assert(m_first_hole);
                }

                // Fills a bucket with holes. Used to reset buckets.
                void fill_bucket_with_holes(size_t bucket_index) {
                    assert(bucket_index > 0 && bucket_index < m_buckets.size()); // Cannot fill bucket 0 because it contains begin and end.
                    auto& bucket = m_buckets[bucket_index];

                    // Clear the bucket.
                    for (size_t i = 0; i < bucket.size(); i++) {
                        bucket[i].next = &bucket[i] + 1;
                        bucket[i].prev = &bucket[i] - 1;
                        bucket[i].elem = std::nullopt;
                    }
                    bucket[0].prev = &m_buckets[bucket_index - 1].back();
                    bucket.back().next = bucket_index == m_buckets.size() - 1 ? nullptr : &m_buckets[bucket_index + 1].front();

                    // Set m_first_hole if needed.
                    if (!m_first_hole)
                        m_first_hole = &bucket[0];
                }

                // Utility function which links prev and next.
                static void link_two_nodes(node* prev, node* next) {
                    if (next) next->prev = prev;
                    if (prev) prev->next = next;
                }

            public:
                // Public types.
                using value_type = T;
                using size_type = size_t;
                using reference = T&;
                using const_reference = const T&;
                using iterator = iterator_impl<T>;
                using const_iterator = iterator_impl<const T>;
                using reverse_iterator = std::reverse_iterator<iterator_impl<T>>;
                using const_reverse_iterator = std::reverse_iterator<iterator_impl<const T>>;


                // Public functions.

                // Constructors.
                vec_list() { m_buckets.emplace_back(2); clear(); }

                template<class it>
                requires(std::forward_iterator<it>)
                vec_list(it first, it last) : vec_list() { insert(begin(), first, last); }
                vec_list(std::initializer_list<T> list) : vec_list(list.begin(), list.end()) {}

                vec_list(size_t count, const T& value) : vec_list() { insert(begin(), count, value); }
                vec_list(size_t count) : vec_list() { for (size_t i = 0; i < count; i++) insert(begin(), T{}); }

                // vec_list is always movable.
                vec_list(vec_list&& other) : vec_list() { *this = std::move(other); }
                vec_list& operator=(vec_list&& other) noexcept {
                    m_buckets = std::move(other.m_buckets); other.m_buckets.clear();
                    m_first_hole = std::exchange(other.m_first_hole, nullptr);
                    m_size = std::exchange(other.m_size, 0);
                    return *this;
                }

                // vec_list is copyable if T is.
                template<class = std::enable_if_t<std::is_copy_assignable_v<T>>>
                vec_list(const vec_list& other) : vec_list() { *this = other; }
                vec_list& operator=(const vec_list& other) {
                    this->clear();
                    this->insert(this->begin(), other.begin(), other.end());
                    return *this;
                }

                // Accessors.
                bool empty() const { return m_size == 0; }
                size_type size() const { return m_size; }
                size_type max_size() const { return m_buckets[0].max_size(); }
                size_type capacity() const { return m_buckets.size() == 1 ? 0 : MIN_BUCKET_SIZE * (size_t)std::pow(GROWTH_FACTOR, m_buckets.size() - 2); }
                
                // Iterators.
                iterator begin() { return iterator(m_buckets[0][1].next); }
                iterator end() { return iterator(&m_buckets[0][0]); }
                const_iterator begin() const { return const_cast<vec_list*>(this)->begin(); }
                const_iterator end() const { return const_cast<vec_list*>(this)->end(); }
                const_iterator cbegin() const { return begin(); }
                const_iterator cend() const { return end(); }

                reverse_iterator rbegin() { return std::make_reverse_iterator(begin()); }
                reverse_iterator rend() { return std::make_reverse_iterator(end()); }
                const_reverse_iterator rbegin() const { return std::make_reverse_iterator(begin()); }
                const_reverse_iterator rend() const { return std::make_reverse_iterator(end()); }
                const_reverse_iterator crbegin() const { return std::make_reverse_iterator(begin()); }
                const_reverse_iterator crend() const { return std::make_reverse_iterator(end()); }

                // Front and back.
                reference front() { return *begin(); }
                const_reference front() const { return *begin(); }
                reference back() { return *std::prev(end()); }
                const_reference back() const { return *std::prev(end()); }

                // Assign.
                template<class it>
                void assign(it first, it last) { clear(); insert(first, last); }
                void assign(size_t count, const T& value) { clear(); insert(end(), count, value); }
                void assign(std::initializer_list<T> list) { assign(list.begin(), list.end()); }

                // Insert.
                template<class it>
                requires(std::forward_iterator<it>)
                void insert(const_iterator pos, it first, it last) { while (first != last) insert(pos, *(first++)); }
                iterator insert(const_iterator pos, T&& value) { return emplace(pos, std::move(value)); }
                iterator insert(const_iterator pos, const T& value) { return emplace(pos, value); }
                void insert(const_iterator pos, size_t count, const T& value) { for (size_t i = 0; i < count; i++) insert(pos, value); }
                void insert(const_iterator pos, std::initializer_list<T> list) { insert(pos, list.begin(), list.end()); }

                // Push and pop.
                template<class... Ts>
                reference emplace_back(Ts&&... args) { return *emplace(end(), std::forward<Ts>(args)...); }
                reference push_back(T&& value) { return emplace_back(std::move(value)); }
                reference push_back(const T& value) { return emplace_back(value); }
                void pop_back() { erase(std::prev(end())); }

                template<class... Ts>
                reference emplace_front(Ts&&... args) { return *emplace(begin(), std::forward<Ts>(args)...); }
                reference push_front(T&& value) { return emplace_front(std::move(value)); }
                reference push_front(const T& value) { return emplace_front(value); }
                void pop_front() { erase(begin()); }

                // Comparisons.
                friend bool operator==(const vec_list& a, const vec_list& b) {
                    if (a.size() != b.size())
                        return false;
                    return a <=> b;
                }

                friend auto operator<=>(const vec_list& a, const vec_list& b) {
                    return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end());
                }

                // Actual emplace function that does all the work.
                template<class... Ts>
                iterator emplace(const_iterator pos, Ts&&... args) {
                    if (m_first_hole == nullptr) {
                        add_bucket();
                    }
                    auto current = m_first_hole;
                    m_first_hole = m_first_hole->next;
                    current->elem.emplace(std::forward<Ts>(args)...);

                    auto prev = pos.m_node->prev;
                    link_two_nodes(current, pos.m_node);
                    link_two_nodes(prev, current);

                    m_size++;
                    return iterator(current);
                }

                // Actual erase function that does all the work.
                iterator erase(const_iterator first, const_iterator last) { while (first != last) { first = erase(first); } return iterator(first.m_node); }
                iterator erase(const_iterator it) {
                    assert(it.m_node && it.m_node->elem.has_value());
                    m_size--;
                    it.m_node->elem = std::nullopt;

                    auto next = it.m_node->next;
                    link_two_nodes(it.m_node->prev, it.m_node->next);
                    link_two_nodes(it.m_node, m_first_hole);
                    m_first_hole = it.m_node;

                    return iterator(next);
                }

                // Clears the list.
                void clear() {
                    for (size_t bucket_index = 1; bucket_index < m_buckets.size(); bucket_index++) {
                        fill_bucket_with_holes(bucket_index);
                    }
                    link_two_nodes(&m_buckets[0][1], &m_buckets[0][0]);
                    m_size = 0;
                }

                // Reserves more memory.
                void reserve(size_t new_capacity) {
                    while (capacity() < new_capacity)
                        add_bucket();
                }

                // Resizes up or down by adding or removing elements at the end.
                void resize(size_t new_size) {
                    while (size() < new_size)
                        emplace_back();
                    while (size() > new_size)
                        pop_back();
                }

                void resize(size_t new_size, const T& value) {
                    while (size() < new_size)
                        emplace_back(value);
                    while (size() > new_size)
                        pop_back();
                }
            };


        } // namespace vec_list_namespace
    } // namespace details


    // Exports.
    using details::vec_list_namespace::vec_list;


} // namespace palla