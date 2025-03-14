/// @file
/// Example of using the cetl::pmr::PolymorphicAllocatorDeleter from cetl/pmr/memory.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// cSpell: words emplacer

#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/interface_ptr.hpp"
#include "cetl/pmr/memory.hpp"

#include <unordered_map>
#include <iostream>

#include <gtest/gtest.h>

class INamed
{
public:
    INamed()                                  = default;
    INamed(const INamed&)                     = delete;
    INamed(INamed&& rhs) noexcept             = delete;
    INamed& operator=(INamed&&)               = delete;
    INamed& operator=(const INamed&) noexcept = delete;

    virtual std::string name() const = 0;

protected:
    ~INamed() = default;
};

class IDescribable : public INamed
{
public:
    IDescribable()                                        = default;
    IDescribable(const IDescribable&)                     = delete;
    IDescribable(IDescribable&& rhs) noexcept             = delete;
    IDescribable& operator=(IDescribable&&)               = delete;
    IDescribable& operator=(const IDescribable&) noexcept = delete;

    virtual std::string describe() const = 0;

protected:
    ~IDescribable() = default;
};

class IIdentifiable
{
public:
    IIdentifiable()                                         = default;
    IIdentifiable(const IIdentifiable&)                     = delete;
    IIdentifiable(IIdentifiable&& rhs) noexcept             = delete;
    IIdentifiable& operator=(IIdentifiable&&)               = delete;
    IIdentifiable& operator=(const IIdentifiable&) noexcept = delete;

    virtual std::uint32_t id() const = 0;

protected:
    ~IIdentifiable() = default;
};

class MyObjectBase
{
public:
    MyObjectBase()
        : id_{counter_++}
    {
    }

protected:
    const std::uint32_t  id_;
    static std::uint32_t counter_;
    friend class example_07_polymorphic_alloc_deleter;
};
std::uint32_t MyObjectBase::counter_ = 0;

class MyObject final : private MyObjectBase, public IIdentifiable, public IDescribable
{
public:
    MyObject(const char* name, std::size_t name_length)
        : name_{static_cast<char*>(malloc(name_length + 1))}
    {
        strncpy(name_, name, name_length + 1);
        name_[name_length] = '\0';
    }

    MyObject(const MyObject&)                = delete;
    MyObject(MyObject&& rhs) noexcept        = delete;
    MyObject& operator=(const MyObject&)     = delete;
    MyObject& operator=(MyObject&&) noexcept = delete;

    ~MyObject()
    {
        std::cout << "~MyObject(name='" << name_ << "', id=" << id_ << ")" << std::endl;
        free(name_);
    }

    // MARK: INamed

    std::string name() const override
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

    // MARK: IIdentifiable

    std::uint32_t id() const override
    {
        return id_;
    }

    // MARK: IDescribable

    std::string describe() const override
    {
        return name() + " is a MyObject instance.";
    }

private:
    char* name_;
};

class example_07_polymorphic_alloc_deleter : public testing::Test
{
protected:
    template <typename Interface>
    using InterfacePtr = cetl::pmr::InterfacePtr<Interface>;

    void SetUp() override
    {
        MyObjectBase::counter_ = 0;
    }
};

TEST_F(example_07_polymorphic_alloc_deleter, example_usage_0)
{
    //![example_usage_0]
    // Let's say you wanted to store a bunch of objects in a container of some sort. You can use the
    // cetl::pmr::PolymorphicDeleter to help you build unique_ptr's like this:

    using MyAllocator = cetl::pmr::polymorphic_allocator<MyObject>;
    using MyDeleter   = cetl::pmr::PolymorphicDeleter<MyAllocator>;
    MyAllocator alloc{cetl::pmr::get_default_resource()};

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
    }  // else, if we're here then exceptions are turned off, but deallocation is always null-safe.

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

TEST_F(example_07_polymorphic_alloc_deleter, example_usage_1)
{
    //![example_usage_1]
    // By using the cetl::pmr::Factory, you can simplify the code from the previous example:

    cetl::pmr::polymorphic_allocator<MyObject> alloc{cetl::pmr::get_default_resource()};

    std::unordered_map<std::string, cetl::pmr::Factory::unique_ptr_t<decltype(alloc)>> objects;
    objects.reserve(6);

    auto object_0 = cetl::pmr::Factory::make_unique(alloc, "object_0", 8U);
    objects.emplace(object_0->name(), std::move(object_0));

    auto object_1 = cetl::pmr::Factory::make_unique(alloc, "object_1", 8U);
    objects.emplace(object_1->name(), std::move(object_1));

    auto object_2 = cetl::pmr::Factory::make_unique(alloc, "object_2", 8U);
    objects.emplace(object_2->name(), std::move(object_2));

    // or even simpler:
    auto emplacer = [&objects, &alloc](const char* name, std::size_t name_length) {
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

TEST_F(example_07_polymorphic_alloc_deleter, example_usage_2)
{
    //![example_usage_2]

    cetl::pmr::polymorphic_allocator<MyObject> alloc{cetl::pmr::get_default_resource()};

    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<MyObject>(alloc, "obj0", 4U);
    std::cout << "Obj0 id  : " << obj0->id() << std::endl;

    // Commented b/c of current limitation of our `cetl::pmr::function`.
    // Probably PMR support is needed at `cetl::unbounded_variant` (which we use inside the `function`),
    // so that it will be possible to nest one deleter inside another one.
    auto obj1 = cetl::pmr::InterfaceFactory::make_unique<IIdentifiable>(alloc, "obj1", 4U);
    {
        std::cout << "Obj1 id  : " << obj1->id() << std::endl;
        obj1.reset();
        std::cout << std::endl;
    }

    auto obj2 = cetl::pmr::InterfaceFactory::make_unique<IDescribable>(alloc, "obj2", 4U);
    {
        std::cout << "Obj2 desc  : " << obj2->describe() << std::endl;
        std::cout << "Obj2 name_a  : " << obj2->name() << std::endl;

        // Such interface ptr upcasting currently is not supported.
        //
        //    auto obj2_named = InterfacePtr<INamed>{std::move(obj2)};
        //    std::cout << "Obj2 name_b  : " << obj2_named->name() << std::endl;
    }

    auto obj3 = cetl::pmr::InterfaceFactory::make_unique<INamed>(alloc, "obj3", 4U);
    {
        std::cout << "Obj3 name  : " << obj3->name() << std::endl;
        std::cout << std::endl;
    }

    //![example_usage_2]
}

//![example_usage_3]

// Note that this concrete type is final because it extends `cetl::rtti` non-virtually which is not reccommended
// for any non-final type. Inversely, the InterfaceFactory only works with non-virtual inheritance of the interface
// used in the InterfacePtr type since a static downcast must be performed by the deleter.
// Finally, this encapsulation technique of befriending the polymorphic allocator will always work with CETL PMR
// but may not work with other standard libraries.
class MyConcreteType final : public cetl::rtti
{
public:
    using ConcreteAllocator = cetl::pf17::pmr::polymorphic_allocator<MyConcreteType>;
    // By making this allocator a friend we ensure that this class can only be created using PMR.
    friend ConcreteAllocator;

    // Since this class's constructor is private the only way to instantiate it is using a ConcreteAllocator. This
    // method bundles that constraint up with the proper RAII semantics making it easy to properly construct
    // MyConcreteType classes and hard to construct them improperly.
    template <typename... Args>
    static cetl::pmr::InterfacePtr<cetl::rtti> make(std::allocator_arg_t, ConcreteAllocator alloc, Args&&... args)
    {
        return cetl::pmr::InterfaceFactory::make_unique<cetl::rtti>(alloc, std::forward<Args>(args)...);
    }

    CETL_NODISCARD void* _cast_(const cetl::type_id&) & noexcept override
    {
        return nullptr;
    }
    CETL_NODISCARD const void* _cast_(const cetl::type_id&) const& noexcept override
    {
        return nullptr;
    }

private:
    MyConcreteType()          = default;
    virtual ~MyConcreteType() = default;
};

//![example_usage_3]

TEST_F(example_07_polymorphic_alloc_deleter, example_usage_3)
{
    auto dark_ptr = MyConcreteType::make(std::allocator_arg,
                                         MyConcreteType::ConcreteAllocator{cetl::pf17::pmr::get_default_resource()});
    static_cast<void>(dark_ptr);
}
