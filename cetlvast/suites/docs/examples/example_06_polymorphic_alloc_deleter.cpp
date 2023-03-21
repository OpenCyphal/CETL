/// @file
/// Example of using the cetl::pmr::PolymorphicAllocatorDeleter from cetl/pmr/memory.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// cSpell: words emplacer

//![example_include]
#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/memory.hpp"
//![example_include]

#include <unordered_map>
#include <iostream>
#include <string.h>

#include <gtest/gtest.h>


class MyObject final
{
public:
    MyObject(const char* name, std::size_t name_length)
        : name_(nullptr)
    {
        name_ = static_cast<char*>(malloc(name_length + 1));
        strncpy(name_, name, name_length);
        name_[name_length] = '\0';
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

TEST(example_06_polymorphic_alloc_deleter, example_usage_0)
{
    //![example_usage_0]
    // Let's say you wanted to store a bunch of objects in a container of some sort. You can use the
    // cetl::pmr::PolymorphicDeleter to help you build unique_ptr's like this:

    using MyAllocator = cetl::pmr::polymorphic_allocator<MyObject>;
    using MyDeleter = cetl::pmr::PolymorphicDeleter<MyAllocator>;
    MyAllocator alloc{cetl::pmr::new_delete_resource()};

    std::unordered_map<std::string, std::unique_ptr<MyObject, MyDeleter>> objects;
    objects.reserve(3);

    // where "MyObject" stores a c string internally so we're creating MyObject and MyObject is malloc'ing an internal
    // buffer to copy the string "object_0" (which is 8-characters long) into. If you run this example you'll see that
    // MyObject's destructor is called properly by the the deleter before it deallocate's the memory using the correct
    // allocator.
    std::unique_ptr<MyObject, MyDeleter> object_0{alloc.allocate(1), MyDeleter{alloc, 1}};
    if (nullptr != object_0)
    {
        alloc.construct(object_0.get(), "object_0", 8U);
        objects.emplace(object_0->name(), std::move(object_0));
    } // else, if we're here then exceptions are turned off, but deallocation is always null-safe.

    std::unique_ptr<MyObject, MyDeleter> object_1{alloc.allocate(1), MyDeleter{alloc, 1}};
    if (nullptr != object_1)
    {
        alloc.construct(object_1.get(), "object_1", 8U);
        objects.emplace(object_1->name(), std::move(object_1));
    }

    std::unique_ptr<MyObject, MyDeleter> object_2{alloc.allocate(1), MyDeleter{alloc, 1}};
    if (nullptr != object_2)
    {
        alloc.construct(object_2.get(), "object_2", 8U);
        objects.emplace(object_2->name(), std::move(object_2));
    }

    for (const auto& pair : objects)
    {
        std::cout << "Object name: " << pair.first << std::endl;
    }
    // But this is a lot of boilerplate code. You can use the cetl::pmr::Factory to help you do this more easily
    // (see next example).

    //![example_usage_0]
}

TEST(example_06_polymorphic_alloc_deleter, example_usage_1)
{
    //![example_usage_1]
    // By using the cetl::pmr::Factory, you can simplify the code from the previous example:

    cetl::pmr::polymorphic_allocator<MyObject> alloc{cetl::pmr::new_delete_resource()};

    std::unordered_map<std::string, cetl::pmr::Factory::unique_ptr_t<decltype(alloc)>> objects;
    objects.reserve(6);

    auto object_0 = cetl::pmr::Factory::make_unique(alloc, "object_0", 8U);
    objects.emplace(object_0->name(), std::move(object_0));

    auto object_1 = cetl::pmr::Factory::make_unique(alloc, "object_1", 8U);
    objects.emplace(object_1->name(), std::move(object_1));

    auto object_2 = cetl::pmr::Factory::make_unique(alloc, "object_2", 8U);
    objects.emplace(object_2->name(), std::move(object_2));

    // or even simpler:
    auto emplacer = [&objects, &alloc](const char* name, std::size_t name_length)
    {
        auto object = cetl::pmr::Factory::make_unique(alloc, name, name_length);
        objects.emplace(object->name(), std::move(object));
    };

    emplacer("object_3", 8U);
    emplacer("object_4", 8U);
    emplacer("object_5", 8U);

    for (const auto& pair : objects)
    {
        std::cout << "Object name: " << pair.first << std::endl;
    }
    // Now as long as the map "objects" is in scope, the MyObject instances will be valid. When the map is destroyed,
    // the objects will be deconstructed using the correct allocator.

    //![example_usage_1]
}
