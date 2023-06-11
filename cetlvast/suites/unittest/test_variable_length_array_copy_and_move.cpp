/// @file
/// Unit tests for the copy and move operations of cetl::VariableLengthArray
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pmr/array_memory_resource.hpp"
#include "cetl/pf17/byte.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include <memory>
#include <vector>
#include <array>
#include <type_traits>

// +---------------------------------------------------------------------------+
// | TEST VALUE TYPES
// +---------------------------------------------------------------------------+

/// The most int-like struct you've ever seen. I mean, why even bother? It's
/// just an int.
struct BoxedInt
{
    BoxedInt() noexcept = default;
    BoxedInt(const BoxedInt&) noexcept = default;
    BoxedInt(BoxedInt&&) noexcept = default;
    BoxedInt& operator=(const BoxedInt&) noexcept = default;
    BoxedInt& operator=(BoxedInt&&) noexcept = default;
    ~BoxedInt() noexcept = default;

    BoxedInt(int value) noexcept
        : value_{value}
    {
    }

    BoxedInt& operator=(int rhs) noexcept
    {
        value_ = rhs;
        return *this;
    }

    operator int() const noexcept
    {
        return value_;
    }

    bool operator==(int rhs) const noexcept
    {
        return value_ == rhs;
    }

    bool operator!=(int rhs) const noexcept
    {
        return value_ != rhs;
    }

private:
    int value_;
};

/// Acts like an int but is not trivially copyable, movable, constructable, nor destructible.
/// It also may throw from any of its operations.
struct NonTrivialBoxedInt
{
    NonTrivialBoxedInt()
        : value_{std::make_unique<int>(0)}
    {
    }
    NonTrivialBoxedInt(int value)
        : value_{std::make_unique<int>(value)}
    {
    }
    NonTrivialBoxedInt(const NonTrivialBoxedInt& rhs)
        : value_{std::make_unique<int>(*rhs.value_)}
    {
    }
    NonTrivialBoxedInt(NonTrivialBoxedInt&& rhs)
        : value_{std::move(rhs.value_)}
    {
    }
    NonTrivialBoxedInt& operator=(int rhs)
    {
        value_ = std::make_unique<int>(rhs);
        return *this;
    }
    NonTrivialBoxedInt& operator=(const NonTrivialBoxedInt& rhs)
    {
        return operator=(*rhs.value_);
    }
    NonTrivialBoxedInt& operator=(NonTrivialBoxedInt&& rhs)
    {
        value_ = std::move(rhs.value_);
        return *this;
    }
    ~NonTrivialBoxedInt() = default;

    operator int() const
    {
        return *value_;
    }

    bool operator==(int rhs) const
    {
        return *value_ == rhs;
    }

    bool operator!=(int rhs) const
    {
        return !operator==(rhs);
    }

private:
    std::unique_ptr<int> value_;
};

// +---------------------------------------------------------------------------+
// | TEST PROTOCOL
// +---------------------------------------------------------------------------+

/// Protocol for the typed test suite
template <
    typename SubjectValueType,
    typename SubjectAllocatorFactoryType,
    typename SourceValueType,
    typename SourceAllocatorFactoryType,
    typename SubjectAllocatorValueType =
        typename std::conditional<std::is_same<bool, SubjectValueType>::value, unsigned char, SubjectValueType>::type,
    typename SourceAllocatorValueType =
        typename std::conditional<std::is_same<bool, SourceValueType>::value, unsigned char, SourceValueType>::type>
struct TypeParamDef
{
    TypeParamDef() = delete;

    using subject = cetlvast::AllocatorTypeParamDef<SubjectAllocatorFactoryType, SubjectAllocatorValueType>;
    using source = cetlvast::AllocatorTypeParamDef<SubjectAllocatorFactoryType, SourceAllocatorValueType>;

    using subject_vla_type = cetl::VariableLengthArray<SubjectValueType, typename subject::allocator_type>;
    using source_vla_type = cetl::VariableLengthArray<SourceValueType, typename source::allocator_type>;

    static constexpr typename subject::allocator_type make_subject_allocator()
    {
        return subject::allocator_factory::template make_allocator<typename subject::allocator_type::value_type>();
    }

    static constexpr typename source::allocator_type make_source_allocator()
    {
        return source::allocator_factory::template make_allocator<typename source::allocator_type::value_type>();
    }

    static constexpr void reset()
    {
        subject::allocator_factory::template reset<typename subject::allocator_type::value_type>();
        source::allocator_factory::template reset<typename source::allocator_type::value_type>();
    }
};

// +---------------------------------------------------------------------------+
// | TEST SUITE
// +---------------------------------------------------------------------------+

template <typename T>
class VLACopyMoveTests : public ::testing::Test
{
protected:
    void TearDown() override
    {
        T::reset();
    }
};

// clang-format off
namespace cetlvast
{
using MyTypes = ::testing::Types<
/*                              source value type | allocator factory                                     | subject val. type         | subject allocator factory           */
/*  0 */  TypeParamDef<int,                         PolymorphicAllocatorNewDeleteFactory,                   int,                        PolymorphicAllocatorNewDeleteFactory>
/*  1 */, TypeParamDef<bool,                        PolymorphicAllocatorNewDeleteFactory,                   bool,                       PolymorphicAllocatorNewDeleteFactory>
/*  2 */, TypeParamDef<BoxedInt,                    PolymorphicAllocatorNewDeleteFactory,                   BoxedInt,                   PolymorphicAllocatorNewDeleteFactory>
/*  3 */, TypeParamDef<NonTrivialBoxedInt,          PolymorphicAllocatorNewDeleteFactory,                   NonTrivialBoxedInt,         PolymorphicAllocatorNewDeleteFactory>
/*  4 */, TypeParamDef<int,                         PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  int,                        PolymorphicAllocatorNewDeleteFactory>
/*  5 */, TypeParamDef<bool,                        PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  bool,                       PolymorphicAllocatorNewDeleteFactory>
/*  6 */, TypeParamDef<BoxedInt,                    PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  BoxedInt,                   PolymorphicAllocatorNewDeleteFactory>
/*  7 */, TypeParamDef<NonTrivialBoxedInt,          PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  NonTrivialBoxedInt,         PolymorphicAllocatorNewDeleteFactory>
/*  8 */, TypeParamDef<int,                         DefaultAllocatorFactory,                                int,                        DefaultAllocatorFactory>
/*  9 */, TypeParamDef<bool,                        DefaultAllocatorFactory,                                bool,                       DefaultAllocatorFactory>
/* 10 */, TypeParamDef<BoxedInt,                    DefaultAllocatorFactory,                                BoxedInt,                   DefaultAllocatorFactory>
/* 11 */, TypeParamDef<NonTrivialBoxedInt,          DefaultAllocatorFactory,                                NonTrivialBoxedInt,         DefaultAllocatorFactory>

>;
}  // namespace cetlvast
// clang-format on

TYPED_TEST_SUITE(VLACopyMoveTests, cetlvast::MyTypes, );

// +---------------------------------------------------------------------------+
// | TEST CASES :: Copy Construction
// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyConstruct)
{
    typename TypeParam::source_vla_type source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);

    typename TypeParam::subject_vla_type subject{source};
    EXPECT_EQ(source.size(), 9);
    EXPECT_EQ(subject.size(), source.size());
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyConstructWithNewAllocator)
{
    typename TypeParam::source_vla_type source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);

    typename TypeParam::subject_vla_type subject{source, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);
    EXPECT_EQ(subject.size(), source.size());
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: Copy Assignment
// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyAssign)
{
    typename TypeParam::subject_vla_type subject{TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);
    EXPECT_EQ(subject.size(), 0);
    EXPECT_NE(subject, source);
    subject = source;
    EXPECT_EQ(subject.size(), 9);
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyAssignReplaceWithLess)
{
    typename TypeParam::subject_vla_type subject{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 4);
    EXPECT_EQ(subject.size(), 9);
    EXPECT_NE(subject, source);
    subject = source;
    EXPECT_EQ(subject.size(), 4);
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyAssignReplaceWithMore)
{
    typename TypeParam::subject_vla_type subject{{0, 1, 0, 1}, TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1, 0, 1, 0, 1}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 8);
    EXPECT_EQ(subject.size(), 4);
    EXPECT_NE(subject, source);
    subject = source;
    EXPECT_EQ(subject.size(), 8);
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, CopyAssignReplaceWithMoreWithAdequateCapacity)
{
    typename TypeParam::subject_vla_type subject{{0, 1, 0, 1}, TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1, 0, 1, 0, 1}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 8);
    EXPECT_EQ(subject.size(), 4);
    EXPECT_NE(subject, source);
    subject.reserve(8);
    subject = source;
    EXPECT_EQ(subject.size(), 8);
    EXPECT_EQ(subject, source);
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: Move Construction
// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, MoveConstruct)
{
    typename TypeParam::source_vla_type source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);

    typename TypeParam::subject_vla_type subject{std::move(source)};
    EXPECT_EQ(source.size(), 0);
    EXPECT_EQ(subject.size(), 9);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, MoveConstructWithNewAllocator)
{
    typename TypeParam::source_vla_type source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);

    typename TypeParam::subject_vla_type subject{std::move(source), TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 0);
    EXPECT_EQ(subject.size(), 9);
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: Move Assignment
// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, MoveAssign)
{
    typename TypeParam::subject_vla_type subject{TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);
    EXPECT_EQ(subject.size(), 0);
    EXPECT_NE(subject, source);
    subject = std::move(source);
    EXPECT_EQ(source.size(), 0);
    EXPECT_EQ(subject.size(), 9);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLACopyMoveTests, MoveAssignWithAdequateCapacity)
{
    typename TypeParam::subject_vla_type subject{TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type  source{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(source.size(), 9);
    EXPECT_EQ(subject.size(), 0);
    EXPECT_NE(subject, source);
    subject.reserve(9);
    subject = std::move(source);
    EXPECT_EQ(source.size(), 0);
    EXPECT_EQ(subject.size(), 9);
}
