/// @file
/// Example of using the cetl::pmr::PolymorphicAllocatorDeleter from cetl/pmr/memory.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

//![example_include]
#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/memory.hpp"
//![example_include]

#include <unordered_map>
#include <iostream>
#include <string.h>

class MyObject final
{
public:
    MyObject(const char* name, std::size_t name_length)
        : name_(nullptr)
    {
        name_ = static_cast<char*>(malloc(name_length + 1));
        strncpy(name_, name, name_length);
    }

    MyObject(const MyObject&)            = delete;
    MyObject& operator=(const MyObject&) = delete;
    MyObject& operator=(MyObject&&)      = delete;

    MyObject(MyObject&& rhs) noexcept
        : name_(rhs.name_)
    {
        rhs.name_ = nullptr;
    }

    ~MyObject()
    {
        std::cout << "MyObject destructor called : " << name_ << std::endl;
        free(name_);
    }

    std::string name() const
    {
        if (name_ == nullptr)
        {
            return std::string{};
        }
        else
        {
            return std::string{name_};
        }
    }

private:
    char* name_;
};

template <typename T>
using PMRDeleter = cetl::pmr::PolymorphicAllocatorDeleter<cetl::pmr::polymorphic_allocator<T>>;
using MemoryResourcePointer = std::unique_ptr<void, cetl::pmr::MemoryResourceDeleter>;

int main()
{
    //![example_usage]
    // Let's say you wanted to store a bunch of objects in a container of some sort. You could do something like this:

    PMRDeleter<MyObject>::allocator alloc{cetl::pmr::new_delete_resource()};

    std::unordered_map<std::string, PMRDeleter<MyObject>::unique_ptr> objects;
    objects.reserve(3);

    auto object_0 = PMRDeleter<MyObject>::make_unique(alloc, "object_0", 8U);
    objects.emplace(object_0->name(), std::move(object_0));

    auto object_1 = PMRDeleter<MyObject>::make_unique(alloc, "object_1", 8U);
    objects.emplace(object_1->name(), std::move(object_1));

    auto object_2 = PMRDeleter<MyObject>::make_unique(alloc, "object_2", 8U);
    objects.emplace(object_2->name(), std::move(object_2));

    for (const auto& pair : objects)
    {
        std::cout << "Object name: " << pair.first << std::endl;
    }
    // Now as long as the map "objects" is in scope, the MyObject instances will be valid. When the map is destroyed,
    // the objects will be deconstructed using the correct allocator.

    //![example_usage]
    return 0;
}
