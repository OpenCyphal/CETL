/// @file
/// Unit tests for cetl/pmr/interface_ptr.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pmr/interface_ptr.hpp>
#include <cetlvast/tracking_memory_resource.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace
{

using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;

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
    friend class TestPmrInterfacePtr;
};
std::uint32_t MyObjectBase::counter_ = 0;

class MyObject final : private MyObjectBase, public IIdentifiable, public IDescribable
{
public:
    MyObject(std::string name)
        : name_{std::move(name)}
    {
    }

    ~MyObject()                              = default;
    MyObject(const MyObject&)                = delete;
    MyObject(MyObject&& rhs) noexcept        = delete;
    MyObject& operator=(const MyObject&)     = delete;
    MyObject& operator=(MyObject&&) noexcept = delete;

    // MARK: INamed

    std::string name() const override
    {
        return name_;
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
    std::string name_;
};

class TestPmrInterfacePtr : public testing::Test
{
protected:
    using pmr = cetl::pmr::memory_resource;

    template <typename Interface>
    using InterfacePtr = cetl::pmr::InterfacePtr<Interface, pmr>;

    void SetUp() override
    {
        MyObjectBase::counter_ = 0;
    }

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    pmr* get_mr() noexcept
    {
        return &mr_;
    }

    cetlvast::TrackingMemoryResource mr_;
};

TEST_F(TestPmrInterfacePtr, make_unique_concrete)
{
    cetl::pmr::polymorphic_allocator<MyObject> alloc{get_mr()};

    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<MyObject>(alloc, "obj0");
    EXPECT_THAT(obj0, NotNull());
    EXPECT_THAT(obj0->name(), "obj0");
    EXPECT_THAT(obj0->describe(), "obj0 is a MyObject instance.");
}

TEST_F(TestPmrInterfacePtr, make_unique_interface)
{
    cetl::pmr::polymorphic_allocator<MyObject> alloc{get_mr()};

    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<IDescribable>(alloc, "obj0");
    EXPECT_THAT(obj0, NotNull());
    EXPECT_THAT(obj0->name(), "obj0");
    EXPECT_THAT(obj0->describe(), "obj0 is a MyObject instance.");

    obj0.reset();
}

TEST_F(TestPmrInterfacePtr, up_cast_interface)
{
    cetl::pmr::polymorphic_allocator<MyObject> alloc{get_mr()};

    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<IDescribable>(alloc, "obj0");
    EXPECT_THAT(obj0, NotNull());
    EXPECT_THAT(obj0->name(), "obj0");
    EXPECT_THAT(obj0->describe(), "obj0 is a MyObject instance.");

    cetl::pmr::InterfacePtr<INamed, cetl::pmr::memory_resource> obj0_named{std::move(obj0)};

    EXPECT_THAT(obj0, IsNull());
    EXPECT_THAT(obj0_named, NotNull());
    EXPECT_THAT(obj0_named->name(), "obj0");

    obj0_named.reset();
}

}  // namespace
