#pragma once

#include <vector>
#include <optional>
#include <algorithm>
#include <concepts>
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
                node* m_first_hole = nullptr;				// First hole. Holes form a forward list embedded within this list. When an element is erased, it becomes the new first hole.
                node* m_last_hole = nullptr;				// Last hole. Used for splicing lists together.
                size_t m_size = 0;							// Number of elements (not holes).
                size_t m_capacity = 0;                      // Number of elements and holes.

                // Expansion constants.
                static constexpr size_t MIN_BUCKET_SIZE = std::max<size_t>(16, 4096 / sizeof(node));
                static constexpr size_t GROWTH_FACTOR = 2;


                // Private functions.

                // Adds a bucket. Used when there is no more space and all holes are filled. 
                // m_first_hole is guaranteed to point to a hole after this.
                void add_bucket() {
                    // Buckets grow geometrically.
                    add_bucket(m_capacity * GROWTH_FACTOR);
                }

                void add_bucket(size_t desired_capacity) {
                    // Much like std::vector, using reserve() should bypass geometric growth.
                    size_t bucket_size = std::max(MIN_BUCKET_SIZE, desired_capacity - m_capacity);
                    m_capacity += bucket_size;

                    m_buckets.emplace_back(bucket_size);
                    fill_bucket_with_holes(m_buckets.size() - 1);
                    if (!m_last_hole)
                        m_last_hole = &m_buckets.back().back();
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

                    // Set m_first_hole/m_last_hole.
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
                    m_last_hole = std::exchange(other.m_last_hole, nullptr);
                    m_size = std::exchange(other.m_size, 0);
                    m_capacity = std::exchange(other.m_capacity, 0);
                    return *this;
                }

                // vec_list is copyable if T is.
                vec_list(const vec_list& other) requires std::copy_constructible<T> : vec_list() { *this = other; }
                vec_list& operator=(const vec_list& other) requires std::copy_constructible<T> {
                    this->clear();
                    this->insert(this->begin(), other.begin(), other.end());
                    return *this;
                }

                // Accessors.
                bool empty() const { return m_size == 0; }
                size_type size() const { return m_size; }
                size_type max_size() const { return m_buckets[0].max_size(); }
                size_type capacity() const { return m_capacity; }
                
                // Iterators.
                iterator begin() { return iterator(m_buckets[0][1].next); }
                iterator end() { return iterator(&m_buckets[0][0]); }
                const_iterator begin() const { return const_cast<vec_list*>(this)->begin(); }
                const_iterator end() const { return const_cast<vec_list*>(this)->end(); }
                const_iterator cbegin() const { return begin(); }
                const_iterator cend() const { return end(); }

                reverse_iterator rbegin() { return std::make_reverse_iterator(end()); }
                reverse_iterator rend() { return std::make_reverse_iterator(begin()); }
                const_reverse_iterator rbegin() const { return std::make_reverse_iterator(end()); }
                const_reverse_iterator rend() const { return std::make_reverse_iterator(begin()); }
                const_reverse_iterator crbegin() const { return std::make_reverse_iterator(end()); }
                const_reverse_iterator crend() const { return std::make_reverse_iterator(begin()); }

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
                    return std::equal(a.begin(), a.end(), b.begin(), b.end());
                }

                friend auto operator<=>(const vec_list& a, const vec_list& b) {
                    return std::lexicographical_compare_three_way(a.begin(), a.end(), b.begin(), b.end());
                }

                // Actual emplace function that does all the work.
                template<class... Ts>
                iterator emplace(const_iterator pos, Ts&&... args) {
                    // If there are no more holes, add a new bucket to create new ones.
                    if (m_first_hole == nullptr)
                        add_bucket();

                    // Fill the first hole. If it is the last one, set the last hole to nullptr.
                    auto current = m_first_hole;
                    m_first_hole = m_first_hole->next;
                    if (m_first_hole == nullptr)
                        m_last_hole = nullptr;

                    // Set the element.
                    current->elem.emplace(std::forward<Ts>(args)...);
                    m_size++;

                    // Link the element to pos.
                    auto prev = pos.m_node->prev;
                    link_two_nodes(current, pos.m_node);
                    link_two_nodes(prev, current);
                    return iterator(current);
                }

                // Actual erase function that does all the work.
                iterator erase(const_iterator first, const_iterator last) { while (first != last) { first = erase(first); } return iterator(first.m_node); }
                iterator erase(const_iterator it) {
                    assert(it.m_node && it.m_node->elem.has_value());

                    // Erase the element.
                    m_size--;
                    it.m_node->elem = std::nullopt;

                    // Link the neighbors together.
                    auto next = it.m_node->next;
                    link_two_nodes(it.m_node->prev, it.m_node->next);
                    link_two_nodes(it.m_node, m_first_hole);

                    // Make the element the first hole.
                    m_first_hole = it.m_node;
                    if (!m_last_hole)
                        m_last_hole = m_first_hole;

                    return iterator(next);
                }

                // Clears the list.
                void clear() {
                    for (size_t bucket_index = 1; bucket_index < m_buckets.size(); bucket_index++) {
                        fill_bucket_with_holes(bucket_index);
                    }
                    link_two_nodes(&m_buckets[0][1], &m_buckets[0][0]);
                    if(m_buckets.size() > 1)
                        m_last_hole = &m_buckets.back().back();
                    m_size = 0;
                }

                // Reserves more memory.
                void reserve(size_t new_capacity) {
                    add_bucket(new_capacity);
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

                // Reverse the list.
                void reverse() {
                    // Reverse the elements only. Holes can stay the same.
                    auto first = begin().m_node->prev;
                    auto last = end().m_node;
                    for (auto current = first->next; current != last; current = current->prev) {
                        std::swap(current->prev, current->next);
                    }
                    std::swap(first->next, last->prev);
                    first->next->prev = first;
                    last->prev->next = last;
                }

                // Splices two lists together.
                void splice(const_iterator pos, vec_list& other) {
                    assert(this != &other);
                    if (other.empty())
                        return;

                    // Insert other's buckets into this.
                    this->m_buckets.insert(m_buckets.end(), std::make_move_iterator(other.m_buckets.begin()) + 1, std::make_move_iterator(other.m_buckets.end()));
                    this->m_size += other.m_size;
                    this->m_capacity += other.m_capacity;

                    // Link the holes.
                    link_two_nodes(this->m_last_hole, other.m_first_hole);
                    this->m_last_hole = other.m_last_hole;

                    // Link the elements to pos.
                    auto prev = pos.m_node->prev;
                    link_two_nodes(other.end().m_node->prev, pos.m_node);
                    link_two_nodes(prev, other.begin().m_node);

                    // Hard reset other.
                    other = vec_list{};
                }
                void splice(const_iterator pos, vec_list&& other) { splice(pos, other); }

                // These splice functions are not optimized like they are for std::list.
                void splice(const_iterator pos, vec_list& other, const_iterator it) {
                    this->insert(pos, std::move(*iterator(it.m_node)));
                    other.erase(it);
                }
                void splice(const_iterator pos, vec_list&& other, const_iterator it) { splice(pos, other, it); }

                void splice(const_iterator pos, vec_list& other, const_iterator first, const_iterator last) {
                    if (first == other.begin() && last == other.end())
                        return splice(pos, other);
                    this->insert(pos, std::make_move_iterator(iterator(first.m_node)), std::make_move_iterator(iterator(last.m_node)));
                    other.erase(first, last);
                }
                void splice(const_iterator pos, vec_list&& other, const_iterator first, const_iterator last) { splice(pos, other, first, last); }


            };


        } // namespace vec_list_namespace
    } // namespace details


    // Exports.
    using details::vec_list_namespace::vec_list;


} // namespace palla