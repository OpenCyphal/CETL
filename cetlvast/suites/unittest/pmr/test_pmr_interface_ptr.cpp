/// @file
/// Unit tests for cetl/pmr/interface_ptr.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pmr/interface_ptr.hpp>
#include <cetlvast/memory_resource_mock.hpp>
#include <cetlvast/tracking_memory_resource.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace
{

using testing::_;
using testing::IsNull;
using testing::Return;
using testing::IsEmpty;
using testing::NotNull;
using testing::StrictMock;

#if defined(__cpp_exceptions)

// Workaround for GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
// Should be used in the tests where exceptions are expected (see `EXPECT_THROW`).
const auto sink = [](auto&&) {};

#endif

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
    MyObject(std::string name, bool throw_on_ctor = false)
        : name_{std::move(name)}
    {
        if (throw_on_ctor)
        {
#if defined(__cpp_exceptions)
            throw std::runtime_error("ctor");
#endif
        }
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

    const INamed& obj0_named = *obj0;

    EXPECT_THAT(obj0, NotNull());
    EXPECT_THAT(obj0_named.name(), "obj0");

    obj0.reset();
}

TEST_F(TestPmrInterfacePtr, make_unique_out_of_memory)
{
    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    cetl::pmr::polymorphic_allocator<MyObject> alloc{&mr_mock};

    EXPECT_CALL(mr_mock, do_allocate(sizeof(MyObject), _)).WillOnce(Return(nullptr));

    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<IDescribable>(alloc, "obj0");
    EXPECT_THAT(obj0, IsNull());
}

TEST_F(TestPmrInterfacePtr, make_unique_myobj_ctor_throws)
{
    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    cetl::pmr::polymorphic_allocator<MyObject> alloc{&mr_mock};

    EXPECT_CALL(mr_mock, do_allocate(sizeof(MyObject), _))
        .WillOnce(
            [this](std::size_t size_bytes, std::size_t alignment) { return mr_.allocate(size_bytes, alignment); });
    EXPECT_CALL(mr_mock, do_deallocate(_, sizeof(MyObject), _))
        .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
            mr_.deallocate(p, size_bytes, alignment);
        });

#if defined(__cpp_exceptions)
    EXPECT_THROW(sink(cetl::pmr::InterfaceFactory::make_unique<INamed>(alloc, "obj0", true)), std::runtime_error);
#else
    auto obj0 = cetl::pmr::InterfaceFactory::make_unique<INamed>(alloc, "obj0", true);
    EXPECT_THAT(obj0, NotNull());
    EXPECT_THAT(obj0->name(), "obj0");
#endif
}

TEST_F(TestPmrInterfacePtr, initially_empty_with_default_deleter)
{
    cetl::pmr::polymorphic_allocator<MyObject> alloc{get_mr()};

    // 1. Create initially empty interface pointer.
    cetl::pmr::InterfacePtr<INamed> obj0;
    EXPECT_THAT(obj0, IsNull());

    // 2. Now assign a new instance.
    obj0 = cetl::pmr::InterfaceFactory::make_unique<INamed>(alloc, "obj0");
    EXPECT_THAT(obj0, NotNull());
}

}  // namespace
