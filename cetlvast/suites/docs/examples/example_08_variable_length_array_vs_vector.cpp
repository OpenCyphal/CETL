/// @file
/// Demonstration of the differences between std::vector and cetl::variable_length_array type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// CSpell: words sayin

#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pf17/array_memory_resource.hpp"
#include "cetl/variable_length_array.hpp"
#include <iostream>
#include <vector>

#include <gtest/gtest.h>

template <typename T>
void add_hello_world(T& container)
{
    container.reserve(12);
    container.push_back('H');
    container.push_back('e');
    container.push_back('l');
    container.push_back('l');
    container.push_back('o');
    container.push_back(' ');
    container.push_back('W');
    container.push_back('o');
    container.push_back('r');
    container.push_back('l');
    container.push_back('d');
}

template <typename T>
void print_container(const T& container)
{
    for (auto c : container)
    {
        std::cout << c;
    }

    std::cout << std::endl;
}

#if defined(__cpp_exceptions)

TEST(example_08_variable_length_array_vs_vector, example_tight_fit_0)
{
    //! [example_tight_fit_0]
    /// while a VariableLengthArray will attempt to grow its capacity using a geometric sequence it will also fit
    /// tightly inside an array. For example, where std::vector, given an allocator with a maximum_size of
    /// 56 bytes, and pushing back 8-byte elements one at at time, some vector implementations will allocate
    /// 8, 16, 32, then 64 bytes. This last allocation, obviously, will fail. A VariableLengthArray allocation
    /// may start with a similar sequence but would never attempt to allocate more than max_size; 8, 16, 32, 56.

    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<56>          array_storage_1{};
    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<56>          array_storage_0{&array_storage_1,
                                                                           array_storage_1.max_size()};
    std::vector<char, cetl::pf17::pmr::polymorphic_allocator<char>> space_waster{{&array_storage_0}};

    try
    {
        for (std::size_t i = 0; i < 56; ++i)
        {
            std::cout << i << ", ";
            space_waster.push_back(static_cast<char>(i + 46));
        }
    } catch (const std::bad_alloc&)
    {
        std::cout << "<- vector claimed to have run out of memory but we know better." << std::endl;
    }
    //! [example_tight_fit_0]
}

TEST(example_08_variable_length_array_vs_vector, example_tight_fit_1)
{
    //! [example_tight_fit_1]
    // The problem with the C++17 standard is the lack of support for max_size in pmr types.
    // VariableLengthArray provides the "max_size_max" argument that lets the user limit the amount of memory
    // the container will use.
    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<56>                        array_storage_1{};
    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<56>                        array_storage_0{&array_storage_1,
                                                                           array_storage_1.max_size()};
    cetl::VariableLengthArray<char, cetl::pf17::pmr::polymorphic_allocator<char>> tight_fit{{&array_storage_0},
                                                                                            array_storage_0.size()};
    for (std::size_t i = 0; i < 56; ++i)
    {
        std::cout << i << ", ";
        tight_fit.push_back(static_cast<char>(i + 46));
    }

    std::cout << "<- The VLA was able to fit tightly inside of the maximum size it was given." << std::endl;
    //! [example_tight_fit_1]
}

TEST(example_08_variable_length_array_vs_vector, example_exact_fit)
{
    //! [example_exact_fit]
    /// Using the reserve function a VariableLengthArray can be made to fit exactly inside of a given memory
    /// resource.

    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<56>                        array_storage_0{};
    cetl::VariableLengthArray<char, cetl::pf17::pmr::polymorphic_allocator<char>> exact_fit{{&array_storage_0},
                                                                                            array_storage_0.size()};
    exact_fit.reserve(56);
    for (std::size_t i = 0; i < 56; ++i)
    {
        std::cout << i << ", ";
        exact_fit.push_back(static_cast<char>(i + 46));
    }

    std::cout << "<- The VLA only used the 56 chars we gave it and no more." << std::endl;
    //! [example_exact_fit]
}

TEST(example_08_variable_length_array_vs_vector, example_no_exceptions)
{
    // compile with -fno-exceptions to enable this example.
}

#else

TEST(example_08_variable_length_array_vs_vector, example_no_exceptions)
{
    //! [example_no_exceptions]

    // Using the cetlpf.hpp header we can create a polymorphic allocator that is aliased to
    // std::pmr::polymorphic_allocator when compiling with C++17 or newer and cetl::pf17::pmr::polymorphic_allocator
    // when compiling with C++14.
    cetl::pf17::pmr::polymorphic_allocator<char> alloc{cetl::pf17::pmr::new_delete_resource()};

    // This allows us to demonstrate that the cetl::variable_length_array behaves like a std::vector...
    std::vector<char, cetl::pf17::pmr::polymorphic_allocator<char>>               a{alloc};
    cetl::VariableLengthArray<char, cetl::pf17::pmr::polymorphic_allocator<char>> b{alloc};

    add_hello_world(a);
    add_hello_world(b);

    print_container(a);
    print_container(b);

    // So why not just use vector?
    // The primary reason is std::vector has some edge cases where it cannot be used when exceptions are disabled.
    // Here we are giving a vector and a VariableLengthArray the same allocator CETL pf17 allocator. This allocator
    // returns nullptr when out of memory and exceptions are disabled.

    cetl::pf17::pmr::polymorphic_allocator<char> bad_alloc{cetl::pf17::pmr::null_memory_resource()};

    std::vector<char, cetl::pf17::pmr::polymorphic_allocator<char>>               bad_a{bad_alloc};
    cetl::VariableLengthArray<char, cetl::pf17::pmr::polymorphic_allocator<char>> bad_b{bad_alloc};

    //  In this case, the VariableLengthArray will fail gracefully when it failed to allocate.
    bad_b.push_back('H');

    // But this would cause undefined behavior in the vector. Some implementations might cause an abort() whereas
    // others may cause a segfault (or worse).

    // of course, we commented out the following line but feel free to uncomment and try yourself.
    // bad_a.push_back('H');

    // You can detect that VariableLengthArray was not able to allocate memory for push_back using the following
    // technique:
    const std::size_t size_before = bad_b.size();
    bad_b.push_back('H');
    if (bad_b.size() == size_before)
    {
        if (size_before == bad_b.max_size())
        {
            std::cout << "bad_b was not able to allocate memory because it reached its max_size. You probably should "
                         "have checked this first? Just sayin'."
                      << std::endl;
        }
        else
        {
            std::cout << "bad_b allocator is out of memory." << std::endl;
        }
    }
    //! [example_no_exceptions]
}
#endif  // __cpp_exceptions
