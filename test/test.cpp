
#include <iostream>
#include <numeric>
#include <random>
#include <chrono>
#include <list>
#include <iomanip>

#include "../header/vec_list.h"

// Console color codes.
namespace colors {
    static const char* const white = "\033[0m";
    static const char* const green = "\033[92m";
    static const char* const yellow = "\033[93m";
    static const char* const red = "\033[91m";
}

// Utility function to terminate the test.
void make_test_fail(const char* text) {
    std::cout << colors::red << "\nFAIL: " << colors::white << text << "\n\n";
    std::exit(0);
}

template<class T, class C, class F>
void verify_vec_list_vs_std_list_stage_2(C&& compare_elements, F&& func) {
    // Apply functo both lists.
    std::list<T> std_list;
    palla::vec_list<T> vec_list;

    func(std_list);
    func(vec_list);

    // Verify the lists are the same.
    if (std_list.empty() != vec_list.empty())
        make_test_fail("The lists are not the same size.");

    if (std_list.size() != vec_list.size())
        make_test_fail("The lists are not the same size.");

    auto std_it = std_list.begin();
    auto vec_it = vec_list.begin();

    for (size_t i = 0; i < std_list.size(); i++) {
        if(!compare_elements(*std_it, *vec_it))
            make_test_fail("The elements are different.");
        ++std_it;
        ++vec_it;
    }

    if(vec_it != vec_list.end())
        make_test_fail("Inconsistent size() and end().");
}

template<class U, class V>
void verify_vec_list_vs_std_list_stage_1(U&& compare_elements, V&& create_vector) {

    using T = std::decay_t<decltype(create_vector()[0])>;
    constexpr bool is_copyable = std::is_copy_assignable_v<T>;

    auto create_list = [&](const auto& reference_list) {
        using list_t = std::decay_t<decltype(reference_list)>;
        auto elems = create_vector();
        if constexpr (is_copyable)
            return list_t(elems.begin(), elems.end());
        else
            return list_t(std::make_move_iterator(elems.begin()), std::make_move_iterator(elems.end()));
    };
    
    // Constructors.

    // list()
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        using list_t = std::decay_t<decltype(list)>;
        list = list_t();
    });

    // list(it begin, it end)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        using list_t = std::decay_t<decltype(list)>;
        auto elems = create_vector();
        if constexpr (is_copyable)
            list = list_t(elems.begin(), elems.end());
        else
            list = list_t(std::make_move_iterator(elems.begin()), std::make_move_iterator(elems.end()));
    });

    // list(size_t count)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        using list_t = std::decay_t<decltype(list)>;
        list = list_t(10);
    });

    // list(list&& other)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        auto other_list = create_list(list);
        list = std::move(other_list);
    });

    if constexpr (is_copyable) {
        // list(const list& other)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            auto other_list = create_list(list);
            list = other_list;
        });

        // list(size_t count, const T& value)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [](auto& list) {
            using list_t = std::decay_t<decltype(list)>;
            list = list_t(10, T{});
        });
    }

    // Insertion.

    // insert(const_iterator pos, it first, it last)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        auto elems = create_vector();
        if constexpr (is_copyable)
            list.insert(std::prev(list.end()), elems.begin(), elems.end());
        else
            list.insert(std::prev(list.end()), std::make_move_iterator(elems.begin()), std::make_move_iterator(elems.end()));
    });

    // insert(const_iterator pos, T&& value)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.insert(std::prev(list.end()), std::move(create_vector()[0]));
    });

    if constexpr (is_copyable) {
        // insert(const_iterator pos, const T& value)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            list = create_list(list);
            list.insert(std::prev(list.end()), create_vector()[0]);
        });

        // insert(const_iterator pos, size_t count, const T& value)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            list = create_list(list);
            list.insert(std::prev(list.end()), 10, create_vector()[0]);
        });
    }

    // emplace(const_iterator pos, Ts&&... args)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.emplace(std::prev(list.end()), std::move(create_vector()[0]));
    });

    // Push and pop.

    // emplace_back(Ts&&... args)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.emplace_back(std::move(create_vector().front()));
    });

    // emplace_front(Ts&&... args)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.emplace_front(std::move(create_vector().back()));
    });

    // push_back(T&& value)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.push_back(std::move(create_vector().front()));
    });

    // push_front(T&& value)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.push_front(std::move(create_vector().back()));
    });

    // pop_back()
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.pop_back();
    });

    // pop_front()
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.pop_front();
    });

    if constexpr (is_copyable) {
        // push_back(const T& value)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            list = create_list(list);
            list.push_back(create_vector().front());
        });

        // push_front(const T& value)
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            list = create_list(list);
            list.push_front(create_vector().back());
        });
    }

    // Erase.
    
    // erase(const_iterator pos)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.erase(std::prev(std::prev(std::prev(list.end()))));
    });

    // erase(const_iterator first, const_iterator last)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.erase(std::next(std::next(list.begin())), std::prev(std::prev(list.end())));
    });

    // clear()
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.clear();
    });

    // Resize.

    // resize(size_t count) (down)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.resize(list.size() / 2);
    });

    // resize(size_t count) (up)
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
        list = create_list(list);
        list.resize(list.size() * 2);
    });

    if constexpr (is_copyable) {
        // resize(size_t count, const T& value) (up).
        verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
            list = create_list(list);
            list.resize(list.size() * 2, create_vector()[0]);
        });
    }

    // Insert and remove lots of random elements.
    verify_vec_list_vs_std_list_stage_2<T>(compare_elements, [&](auto& list) {
 
        constexpr size_t MAX_SIZE = 64;
        constexpr size_t NB_STEPS = 100000;
        std::minstd_rand rand(42);

        using it = std::decay_t<decltype(list.begin())>;
        std::vector<it> iterators;

        for (size_t i = 0; i < NB_STEPS; i++) {

            bool do_erase = std::uniform_real_distribution(0.0, 1.0)(rand) < (double)list.size() / MAX_SIZE;
            if (do_erase) {
                size_t index = std::uniform_int_distribution<size_t>(0, list.size() - 1)(rand);
                auto iterator = iterators[index];
                iterators.erase(iterators.begin() + index);
                list.erase(iterator);
            }
            else {
                auto elems = create_vector();
                auto& elem = elems[std::uniform_int_distribution<size_t>(0, elems.size() - 1)(rand)];
                size_t index = std::uniform_int_distribution<size_t>(0, list.size())(rand);
                auto iterator = index == list.size() ? list.end() : iterators[index];
                iterators.insert(iterators.begin() + index, list.insert(iterator, std::move(elem)));
            }
        }
    });
}

void test_consistency_with_std_list() {
    std::cout << "\nTesting consistency with std::list.\n" << colors::yellow << "TESTING..." << colors::white << '\r';

    // Test a trivial type.
    verify_vec_list_vs_std_list_stage_1([](int a, int b) { return a == b; }, []() {
        std::vector<int> vec(10);
        std::iota(vec.begin(), vec.end(), 0);
        return vec;
    });

    // Test a move-only type.
    verify_vec_list_vs_std_list_stage_1([](const std::unique_ptr<int>& a, const std::unique_ptr<int>& b) { return (a == nullptr && b == nullptr) || (*a == *b); }, []() {
        std::vector<std::unique_ptr<int>> vec(10);
        vec[0] = std::make_unique<int>(1);
        vec[3] = std::make_unique<int>(42);
        vec[5] = std::make_unique<int>(69);
        vec[9] = std::make_unique<int>(1337);
        return vec;
    });

    std::cout << colors::green << "PASS              " << colors::white;
}

template<class T>
std::chrono::duration<double> bench_insertion(int nb_elems) {
    T list;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < nb_elems; i++) {
        list.insert(list.begin(), i);
    }
    auto end = std::chrono::steady_clock::now();
    return end - start;
}

void test_performance() {
    std::cout << "\nBenchmark:\n";

    // Compare insertion speed vs std::list.
    constexpr double margin_of_error = 0.2;
    constexpr int col_width = 20;
    std::cout << " number of elements inserted |     time for std::list      |     time for vec_list       \n";
    std::cout << "-----------------------------|-----------------------------|-----------------------------\n";
    for (int nb_elems = 1000; nb_elems <= 10000000; nb_elems *= 10) {
        auto std_list_time = bench_insertion<std::list<int>>(nb_elems);
        auto vec_list_time = bench_insertion<palla::vec_list<int>>(nb_elems);

        const char* std_list_color = colors::yellow;
        const char* vec_list_color = colors::yellow;
        if (std_list_time * (1 + margin_of_error) < vec_list_time) {
            std_list_color = colors::green;
            vec_list_color = colors::red;
        }
        else if (vec_list_time * (1 + margin_of_error) < std_list_time) {
            std_list_color = colors::red;
            vec_list_color = colors::green;
        }

        std::cout << std::setw(col_width) << nb_elems << "         |";
        std::cout << std_list_color << std::setw(col_width) << std_list_time << colors::white << "         |";
        std::cout << vec_list_color << std::setw(col_width) << vec_list_time << colors::white << '\n';
    }
}



int main() {

    std::cout << colors::white;

    test_consistency_with_std_list();

    std::cout << "\n\nGlobal Result: " << colors::green << "PASS" << colors::white << "\n";

    test_performance();

    return 0;
}