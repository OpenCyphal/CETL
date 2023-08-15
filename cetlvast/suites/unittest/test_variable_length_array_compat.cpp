/// @file
/// Unit tests that confirm cetl::VariableLengthArray behaviour versus std::vector.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words wself

#include "cetl/variable_length_array.hpp"

#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"
#include "cetl/pf17/byte.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"

#include <memory>
#include <type_traits>
#include <limits>
#include <string>
#include <array>
#include <stdexcept>
#include <vector>
#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

// +-------------------------------------------------------------------------------------------------------------------+
// | INTEGER LIKE PRIMITIVE TYPES
// +-------------------------------------------------------------------------------------------------------------------+

/**
 * Test suite to ensure primitive type compatibility.
 */
template <typename T>
class VLATestsCompatPrimitiveTypes : public ::testing::Test
{};
using VLATestsCompatPrimitiveTypesTypes = ::testing::Types<int, std::uint64_t, char, bool>;
TYPED_TEST_SUITE(VLATestsCompatPrimitiveTypes, VLATestsCompatPrimitiveTypesTypes, );

TYPED_TEST(VLATestsCompatPrimitiveTypes, TestMoveToVector)
{
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>>
        subject{10u, cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource())};
    subject.reserve(subject.max_size());
    ASSERT_EQ(subject.capacity(), subject.max_size());
    for (std::size_t i = 0; i < subject.max_size(); ++i)
    {
        subject.push_back(static_cast<TypeParam>(i % 2));
        ASSERT_EQ(i + 1, subject.size());
    }
    std::vector<TypeParam> a(subject.cbegin(), subject.cend());
    for (std::size_t i = 0; i < subject.max_size(); ++i)
    {
        ASSERT_EQ(static_cast<TypeParam>(i % 2), a[i]);
    }
}

TYPED_TEST(VLATestsCompatPrimitiveTypes, TestPushBackGrowsCapacity)
{
    static constexpr std::size_t                                                            PushBackItems = 9;
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>> subject{
        cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource())};

    ASSERT_EQ(0U, subject.size());
    ASSERT_EQ(0U, subject.capacity());
    for (std::size_t i = 0; i < PushBackItems; ++i)
    {
        ASSERT_EQ(i, subject.size());
        ASSERT_LE(i, subject.capacity());
        subject.push_back(static_cast<TypeParam>(i));
        ASSERT_EQ(i + 1, subject.size());
        ASSERT_LE(i + 1, subject.capacity());
    }
    ASSERT_EQ(PushBackItems, subject.size());
    ASSERT_LE(PushBackItems, subject.capacity());
}

TYPED_TEST(VLATestsCompatPrimitiveTypes, TestForEachConstIterators)
{
    static constexpr std::size_t MaxSize = 9;
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>>
        subject{MaxSize, cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource())};
    const auto& const_subject = subject;
    ASSERT_EQ(0U, const_subject.size());
    ASSERT_EQ(0U, const_subject.capacity());
    for (const auto& item : const_subject)  // Requires begin() const, end() const.
    {
        (void) item;
        FAIL();
    }
    ASSERT_EQ(0U, const_subject.size());
    ASSERT_EQ(0U, const_subject.capacity());
    for (std::size_t i = 0; i < MaxSize; ++i)
    {
        ASSERT_EQ(i, const_subject.size());
        ASSERT_LE(i, const_subject.capacity());
        subject.push_back(static_cast<TypeParam>(i % 2));
        ASSERT_EQ(i + 1, const_subject.size());
        ASSERT_LE(i + 1, const_subject.capacity());
    }
    ASSERT_EQ(MaxSize, const_subject.size());
    ASSERT_LE(MaxSize, const_subject.capacity());
    std::size_t i = 0;
    for (const auto& item : const_subject)  // Requires begin() const, end() const.
    {
        ASSERT_EQ(static_cast<TypeParam>(i % 2), item);
        ++i;
    }
    ASSERT_EQ(const_subject.size(), i);
}

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wself-assign-overloaded"
#endif

TYPED_TEST(VLATestsCompatPrimitiveTypes, SelfAssignment)
{
    auto allocator = cetl::pf17::pmr::polymorphic_allocator<TypeParam>(cetl::pf17::pmr::new_delete_resource());
    cetl::VariableLengthArray<TypeParam, cetl::pf17::pmr::polymorphic_allocator<TypeParam>> subject{allocator};
    subject.push_back(0);
    subject.push_back(1);
    ASSERT_EQ(2U, subject.size());
    subject = subject;
    ASSERT_EQ(2U, subject.size());
    ASSERT_EQ(0, subject[0]);
    ASSERT_EQ(1, subject[1]);
}

#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

TYPED_TEST(VLATestsCompatPrimitiveTypes, TestAssignCountItems)
{
    std::allocator<TypeParam>                                 allocator{};
    cetl::VariableLengthArray<TypeParam, std::allocator<TypeParam>> subject{allocator};
    const TypeParam value0 = std::numeric_limits<TypeParam>::max();
    const TypeParam value1 = std::numeric_limits<TypeParam>::min();
    subject.assign(16, value0);
    ASSERT_EQ(16, subject.size());
    for (auto i = subject.begin(), e = subject.end(); i != e; ++i)
    {
        ASSERT_EQ(*i, value0);
    }
    subject.assign(32, value1);
    ASSERT_EQ(32, subject.size());
    for (auto i = subject.begin(), e = subject.end(); i != e; ++i)
    {
        ASSERT_EQ(*i, value1);
    }
}

// +-------------------------------------------------------------------------------------------------------------------+
// | ANY TYPE
// |    These are just the rest of the tests. All ad-hoc and simple.
// +-------------------------------------------------------------------------------------------------------------------+

TEST(VLATestsCompatAnyType, TestDeallocSizeNonBool)
{
    cetlvast::InstrumentedAllocatorStatistics&          stats = cetlvast::InstrumentedAllocatorStatistics::get();
    cetlvast::InstrumentedNewDeleteAllocator<int>       allocator;
    cetl::VariableLengthArray<int, decltype(allocator)> subject{allocator};

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    ASSERT_EQ(1U, stats.allocations);
    ASSERT_EQ(10U * sizeof(int), stats.last_allocation_size_bytes);
    ASSERT_EQ(0U, stats.last_deallocation_size_bytes);
    subject.pop_back();
    subject.shrink_to_fit();
    ASSERT_EQ(10U * sizeof(int), stats.last_deallocation_size_bytes);
}

TEST(VLATestsCompatAnyType, TestPush)
{
    cetlvast::InstrumentedAllocatorStatistics&            stats = cetlvast::InstrumentedAllocatorStatistics::get();
    cetlvast::InstrumentedNewDeleteAllocator<std::size_t> allocator;
    cetl::VariableLengthArray<std::size_t, decltype(allocator)> subject{allocator};
    ASSERT_EQ(nullptr, subject.data());
    ASSERT_EQ(0U, subject.size());
    std::size_t x = 0;
    for (std::size_t i = 0; i < 1024U; ++i)
    {
        subject.push_back(x);
        ASSERT_EQ(i + 1, subject.size());
        ASSERT_LE(subject.size(), subject.capacity());
        const std::size_t* const pushed = &subject[i];
        ASSERT_EQ(*pushed, x);
        ++x;
    }
    // The container must not allocate for each push.
    ASSERT_GT(1024U, stats.allocations);
    subject.clear();
    ASSERT_EQ(0U, subject.size());
    ASSERT_GE(subject.capacity(), 1024U);
    subject.shrink_to_fit();
    ASSERT_EQ(0U, subject.capacity());
    ASSERT_EQ(0U, stats.outstanding_allocated_memory);
}

/**
 * Used to test that destructors were called.
 */
class Doomed
{
public:
    Doomed(int* out_signal_dtor)
        : out_signal_dtor_(out_signal_dtor)
        , moved_(false)
    {
    }
    Doomed(Doomed&& from) noexcept
        : out_signal_dtor_(from.out_signal_dtor_)
        , moved_(false)
    {
        from.moved_ = true;
    }
    Doomed(const Doomed&)            = delete;
    Doomed& operator=(const Doomed&) = delete;
    Doomed& operator=(Doomed&&)      = delete;

    ~Doomed()
    {
        if (!moved_)
        {
            (*out_signal_dtor_) += 1;
        }
    }

private:
    int* out_signal_dtor_;
    bool moved_;
};

TEST(VLATestsCompatAnyType, TestDestroy)
{
    int dtor_called = 0;

    auto subject =
        std::make_shared<cetl::VariableLengthArray<Doomed, std::allocator<Doomed>>>(std::allocator<Doomed>{});

    subject->reserve(10);
    ASSERT_EQ(10U, subject->capacity());
    subject->push_back(Doomed(&dtor_called));
    ASSERT_EQ(1U, subject->size());
    subject->push_back(Doomed(&dtor_called));
    ASSERT_EQ(2U, subject->size());
    ASSERT_EQ(0, dtor_called);
    subject.reset();
    ASSERT_EQ(2, dtor_called);
}

TEST(VLATestsCompatAnyType, TestNonFundamental)
{
    int dtor_called = 0;

    cetl::VariableLengthArray<Doomed, std::allocator<Doomed>> subject(std::allocator<Doomed>{});

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    subject.push_back(Doomed(&dtor_called));
    ASSERT_EQ(1U, subject.size());
    subject.pop_back();
    ASSERT_EQ(1, dtor_called);
}

TEST(VLATestsCompatAnyType, TestNotMovable)
{
    class NotMovable
    {
    public:
        NotMovable() {}
        NotMovable(NotMovable&&) = delete;
        NotMovable(const NotMovable& rhs) noexcept
        {
            (void) rhs;
        }
    };
    cetl::VariableLengthArray<NotMovable, std::allocator<NotMovable>> subject(std::allocator<NotMovable>{});

    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    NotMovable source;
    subject.push_back(source);
    ASSERT_EQ(1U, subject.size());
}

TEST(VLATestsCompatAnyType, TestMovable)
{
    class Movable
    {
    public:
        Movable(int data)
            : data_(data)
        {
        }
        Movable(const Movable&) = delete;
        Movable(Movable&& move_from) noexcept
            : data_(move_from.data_)
        {
            move_from.data_ = 0;
        }
        int get_data() const
        {
            return data_;
        }

    private:
        int data_;
    };
    cetl::VariableLengthArray<Movable, std::allocator<Movable>> subject(std::allocator<Movable>{});
    subject.reserve(10U);
    ASSERT_EQ(10U, subject.capacity());
    subject.push_back(Movable(1));
    ASSERT_EQ(1U, subject.size());
    Movable* pushed = &subject[0];
    ASSERT_NE(nullptr, pushed);
    ASSERT_EQ(1, pushed->get_data());
}

TEST(VLATestsCompatAnyType, TestInitializerArray)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
}

TEST(VLATestsCompatAnyType, TestCopyConstructor)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> fixture{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};

    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject(fixture);
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
}

TEST(VLATestsCompatAnyType, TestMoveConstructor)
{
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> fixture{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1},
                                                                                std::allocator<std::size_t>{}};

    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> subject(std::move(fixture));
    ASSERT_EQ(10U, subject.size());
    for (std::size_t i = 0; i < subject.size(); ++i)
    {
        ASSERT_EQ(subject.size() - i, subject[i]);
    }
    ASSERT_EQ(0U, fixture.size());
    ASSERT_EQ(0U, fixture.capacity());
}

TEST(VLATestsCompatAnyType, TestCompare)
{
    std::allocator<std::size_t>                                         allocator{};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> one{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> two{{10, 9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    cetl::VariableLengthArray<std::size_t, std::allocator<std::size_t>> three{{9, 8, 7, 6, 5, 4, 3, 2, 1}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsCompatAnyType, TestFPCompare)
{
    std::allocator<std::size_t>                                    allocator{};
    cetl::VariableLengthArray<double, std::allocator<std::size_t>> one{{1.00, 2.00}, allocator};
    cetl::VariableLengthArray<double, std::allocator<std::size_t>> two{{1.00, 2.00}, allocator};
    const double epsilon_for_two_comparison = std::nextafter(4.00, INFINITY) - 4.00;
    cetl::VariableLengthArray<double, std::allocator<std::size_t>>
        three{{1.00, std::nextafter(2.00 + epsilon_for_two_comparison, INFINITY)}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsCompatAnyType, TestCompareBool)
{
    std::allocator<bool>                                  allocator{};
    cetl::VariableLengthArray<bool, std::allocator<bool>> one{{true, false, true}, allocator};
    cetl::VariableLengthArray<bool, std::allocator<bool>> two{{true, false, true}, allocator};
    cetl::VariableLengthArray<bool, std::allocator<bool>> three{{true, true, false}, allocator};
    ASSERT_EQ(one, one);
    ASSERT_EQ(one, two);
    ASSERT_NE(one, three);
}

TEST(VLATestsCompatAnyType, TestCopyAssignment)
{
    std::allocator<double>                                    allocator{};
    cetl::VariableLengthArray<double, std::allocator<double>> lhs{{1.00}, allocator};
    cetl::VariableLengthArray<double, std::allocator<double>> rhs{{2.00, 3.00}, allocator};
    ASSERT_EQ(1U, lhs.size());
    ASSERT_EQ(2U, rhs.size());
    ASSERT_NE(lhs, rhs);
    lhs = rhs;
    ASSERT_EQ(2U, lhs.size());
    ASSERT_EQ(2U, rhs.size());
    ASSERT_EQ(lhs, rhs);
}

TEST(VLATestsCompatAnyType, TestMoveAssignment)
{
    std::allocator<std::string>                                         allocator{};
    cetl::VariableLengthArray<std::string, std::allocator<std::string>> lhs{{std::string("one"), std::string("two")},
                                                                            allocator};
    cetl::VariableLengthArray<std::string, std::allocator<std::string>> rhs{{std::string("three"),
                                                                             std::string("four"),
                                                                             std::string("five")},
                                                                            allocator};
    ASSERT_EQ(2U, lhs.size());
    ASSERT_EQ(3U, rhs.size());
    ASSERT_NE(lhs, rhs);
    lhs = std::move(rhs);
    ASSERT_EQ(3U, lhs.size());
    ASSERT_EQ(0U, rhs.size());
    ASSERT_EQ(0U, rhs.capacity());
    ASSERT_NE(lhs, rhs);
    ASSERT_EQ(std::string("three"), lhs[0]);
}

struct NoDefault
{
    ~NoDefault() = default;
    NoDefault()  = delete;
    NoDefault(int value)
        : data_{value} {};
    NoDefault(const NoDefault&)                = default;
    NoDefault(NoDefault&&) noexcept            = default;
    NoDefault& operator=(const NoDefault&)     = default;
    NoDefault& operator=(NoDefault&&) noexcept = default;

    int data() const noexcept
    {
        return data_;
    }

private:
    int data_;
};

TEST(VLATestsCompatAnyType, TestResizeWithNoDefaultCtorData)
{
    std::allocator<NoDefault>                                       allocator{};
    cetl::VariableLengthArray<NoDefault, std::allocator<NoDefault>> subject{{NoDefault{1}}, allocator};
    ASSERT_EQ(1, subject.size());
    subject.resize(10, NoDefault{2});
    ASSERT_EQ(10, subject.size());
    ASSERT_EQ(1, subject[0].data());
    for (std::size_t i = 1; i < subject.size(); ++i)
    {
        ASSERT_EQ(2, subject[i].data());
    }
}

#ifdef __cpp_exceptions

struct GrenadeError : public std::runtime_error
{
    GrenadeError(const char* message)
        : std::runtime_error(message)
    {
    }
};

struct Grenade
{
    Grenade(int value)
        : value_{value}
    {
    }

    Grenade(const Grenade& rhs)
        : value_{rhs.value_}
    {
        if (value_ == 2)
        {
            throw GrenadeError("Kaboom!");
        }
    }
private:
    int value_;
};

TEST(VLATestsCompatAnyType, TestResizeExceptionFromCtorOnResize)
{
    std::allocator<Grenade>                                     allocator{};
    cetl::VariableLengthArray<Grenade, std::allocator<Grenade>> subject{{Grenade{1}}, allocator};
    ASSERT_EQ(1, subject.size());
    ASSERT_THROW(subject.resize(2, Grenade{2}), GrenadeError);
}

#endif  // __cpp_exceptions

TEST(VLATestsCompatAnyType, TestAssignCountItems)
{
    std::allocator<std::string>                                 allocator{};
    cetl::VariableLengthArray<std::string, std::allocator<std::string>> subject{allocator};
    subject.assign(25, "Hi müm");
    ASSERT_EQ(25, subject.size());
    for (auto i = subject.begin(), e = subject.end(); i != e; ++i)
    {
        ASSERT_EQ(*i, "Hi müm");
    }
    subject.assign(7, "ciao");
    ASSERT_GT(subject.capacity(), 7) << "Assign should not shrink capacity.";
    ASSERT_EQ(7, subject.size());
    for (auto i = subject.begin(), e = subject.end(); i != e; ++i)
    {
        ASSERT_EQ(*i, "ciao");
    }
}
