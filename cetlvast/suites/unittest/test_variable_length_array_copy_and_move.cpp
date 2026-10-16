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
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"
#include "cetl/pf17/byte.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include <memory>
#include <vector>
#include <array>
#include <type_traits>
#include <new>

#if defined(__cpp_exceptions)

namespace
{
struct MoveConstructorAllocatorState
{
    bool        fail_allocation         = false;
    std::size_t allocation_attempts     = 0;
    std::size_t outstanding_allocations = 0;
};

// Never always-equal. Pocma selects propagate_on_container_move_assignment, which must have no bearing on
// allocator-extended move construction.
template <typename T, typename Pocma>
struct MoveConstructorAllocator
{
    using value_type                             = T;
    using is_always_equal                        = std::false_type;
    using propagate_on_container_move_assignment = Pocma;

    explicit MoveConstructorAllocator(MoveConstructorAllocatorState& state) noexcept
        : state_(&state)
    {
    }

    template <typename U>
    MoveConstructorAllocator(const MoveConstructorAllocator<U, Pocma>& rhs) noexcept
        : state_(rhs.state_)
    {
    }

    T* allocate(std::size_t count)
    {
        ++state_->allocation_attempts;
        if (state_->fail_allocation)
        {
            throw std::bad_alloc();
        }
        T* const result = std::allocator<T>{}.allocate(count);
        ++state_->outstanding_allocations;
        return result;
    }

    void deallocate(T* pointer, std::size_t count) noexcept
    {
        if (pointer != nullptr)
        {
            EXPECT_GT(state_->outstanding_allocations, 0U);
            --state_->outstanding_allocations;
            std::allocator<T>{}.deallocate(pointer, count);
        }
    }

    template <typename U>
    bool operator==(const MoveConstructorAllocator<U, Pocma>& rhs) const noexcept
    {
        return state_ == rhs.state_;
    }

    template <typename U>
    bool operator!=(const MoveConstructorAllocator<U, Pocma>& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    MoveConstructorAllocatorState* state_;
};

template <typename T, typename Pocma>
struct MoveConstructorParams
{
    using value_type = T;
    using pocma      = Pocma;
};

template <typename Params>
class VLAMoveConstructorExceptionTests : public ::testing::Test
{
protected:
    using T         = typename Params::value_type;
    using Subject   = cetl::VariableLengthArray<T, MoveConstructorAllocator<T, typename Params::pocma>>;
    using Allocator = typename Subject::allocator_type;
};

using MoveConstructorValueTypes = ::testing::Types<MoveConstructorParams<int, std::false_type>,
                                                   MoveConstructorParams<bool, std::false_type>,
                                                   MoveConstructorParams<int, std::true_type>,
                                                   MoveConstructorParams<bool, std::true_type>>;
TYPED_TEST_SUITE(VLAMoveConstructorExceptionTests, MoveConstructorValueTypes, );

TYPED_TEST(VLAMoveConstructorExceptionTests, UnequalAllocatorAllocationFailure)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    MoveConstructorAllocatorState source_state;
    MoveConstructorAllocatorState destination_state;
    {
        Subject    source{{1, 0, 1}, Allocator{source_state}};
        const auto original_capacity      = source.capacity();
        destination_state.fail_allocation = true;

        EXPECT_THROW((Subject{std::move(source), Allocator{destination_state}}), std::bad_alloc);
        ASSERT_EQ(source.size(), 3U);
        EXPECT_EQ(source.capacity(), original_capacity);
        EXPECT_EQ(source[0], 1);
        EXPECT_EQ(source[1], 0);
        EXPECT_EQ(source[2], 1);
        EXPECT_EQ(source_state.outstanding_allocations, 1U);
        EXPECT_EQ(destination_state.allocation_attempts, 1U);
        EXPECT_EQ(destination_state.outstanding_allocations, 0U);

        // The failed allocation must leave the source available for a later move.
        destination_state.fail_allocation = false;
        Subject destination{std::move(source), Allocator{destination_state}};
        ASSERT_EQ(destination.size(), 3U);
        EXPECT_EQ(destination[0], 1);
        EXPECT_EQ(destination[1], 0);
        EXPECT_EQ(destination[2], 1);
        EXPECT_TRUE(source.empty());
        EXPECT_EQ(source.capacity(), original_capacity);
        EXPECT_EQ(source_state.outstanding_allocations, 1U);
        EXPECT_EQ(destination_state.allocation_attempts, 2U);
        EXPECT_EQ(destination_state.outstanding_allocations, 1U);

        // The successful retry retains source storage regardless of POCMA. Release it explicitly without allocating.
        const auto source_allocation_attempts = source_state.allocation_attempts;
        source.shrink_to_fit();
        EXPECT_EQ(source.capacity(), 0U);
        EXPECT_EQ(source_state.outstanding_allocations, 0U);
        EXPECT_EQ(source_state.allocation_attempts, source_allocation_attempts);
    }
    EXPECT_EQ(source_state.outstanding_allocations, 0U);
    EXPECT_EQ(destination_state.outstanding_allocations, 0U);
}

TYPED_TEST(VLAMoveConstructorExceptionTests, EqualAllocatorDoesNotAllocate)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    MoveConstructorAllocatorState state;
    {
        Subject source{{1, 0, 1}, Allocator{state}};
        state.fail_allocation = true;
        Subject destination{std::move(source), Allocator{state}};

        EXPECT_TRUE(source.empty());
        ASSERT_EQ(destination.size(), 3U);
        EXPECT_EQ(destination[0], 1);
        EXPECT_EQ(destination[1], 0);
        EXPECT_EQ(destination[2], 1);
        EXPECT_EQ(state.allocation_attempts, 1U);
        EXPECT_EQ(state.outstanding_allocations, 1U);
    }
    EXPECT_EQ(state.outstanding_allocations, 0U);
}
}  // namespace

#endif  // __cpp_exceptions

// +---------------------------------------------------------------------------+
// | TEST VALUE TYPES
// +---------------------------------------------------------------------------+

/// The most int-like struct you've ever seen. I mean, why even bother? It's
/// just an int.
struct BoxedInt
{
    BoxedInt() noexcept                           = default;
    BoxedInt(const BoxedInt&) noexcept            = default;
    BoxedInt(BoxedInt&&) noexcept                 = default;
    BoxedInt& operator=(const BoxedInt&) noexcept = default;
    BoxedInt& operator=(BoxedInt&&) noexcept      = default;
    ~BoxedInt() noexcept                          = default;

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
    using source  = cetlvast::AllocatorTypeParamDef<SubjectAllocatorFactoryType, SourceAllocatorValueType>;

    using subject_vla_type = cetl::VariableLengthArray<SubjectValueType, typename subject::allocator_type>;
    using source_vla_type  = cetl::VariableLengthArray<SourceValueType, typename source::allocator_type>;

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
/*  1 */, TypeParamDef<char,                        PolymorphicAllocatorNewDeleteFactory,                   char,                       PolymorphicAllocatorNewDeleteFactory>
/*  2 */, TypeParamDef<bool,                        PolymorphicAllocatorNewDeleteFactory,                   bool,                       PolymorphicAllocatorNewDeleteFactory>
/*  3 */, TypeParamDef<BoxedInt,                    PolymorphicAllocatorNewDeleteFactory,                   BoxedInt,                   PolymorphicAllocatorNewDeleteFactory>
/*  4 */, TypeParamDef<NonTrivialBoxedInt,          PolymorphicAllocatorNewDeleteFactory,                   NonTrivialBoxedInt,         PolymorphicAllocatorNewDeleteFactory>
/*  5 */, TypeParamDef<int,                         PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  int,                        PolymorphicAllocatorNewDeleteFactory>
/*  6 */, TypeParamDef<char,                        PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  char,                       PolymorphicAllocatorNewDeleteFactory>
/*  7 */, TypeParamDef<bool,                        PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  bool,                       PolymorphicAllocatorNewDeleteFactory>
/*  8 */, TypeParamDef<BoxedInt,                    PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  BoxedInt,                   PolymorphicAllocatorNewDeleteFactory>
/*  9 */, TypeParamDef<NonTrivialBoxedInt,          PolymorphicAllocatorNewDeleteBackedMonotonicFactory<>,  NonTrivialBoxedInt,         PolymorphicAllocatorNewDeleteFactory>
/* 10 */, TypeParamDef<int,                         DefaultAllocatorFactory,                                int,                        DefaultAllocatorFactory>
/* 11 */, TypeParamDef<char,                        DefaultAllocatorFactory,                                char,                       DefaultAllocatorFactory>
/* 12 */, TypeParamDef<bool,                        DefaultAllocatorFactory,                                bool,                       DefaultAllocatorFactory>
/* 13 */, TypeParamDef<BoxedInt,                    DefaultAllocatorFactory,                                BoxedInt,                   DefaultAllocatorFactory>
/* 14 */, TypeParamDef<NonTrivialBoxedInt,          DefaultAllocatorFactory,                                NonTrivialBoxedInt,         DefaultAllocatorFactory>

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

TYPED_TEST(VLACopyMoveTests, CopyAssignValueMethod)
{
    typename TypeParam::subject_vla_type            subject{{0, 1, 0, 1}, TypeParam::make_subject_allocator()};
    typename TypeParam::source_vla_type::value_type source{1};
    subject.assign(6, source);
    std::for_each(subject.cbegin(), subject.cend(), [](const typename TypeParam::source_vla_type::value_type& value) {
        ASSERT_EQ(value, 1);
    });
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

// +---------------------------------------------------------------------------+

#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    if __GNUC__ >= 13
#        pragma GCC diagnostic ignored "-Wself-move"
#    endif
#endif

TYPED_TEST(VLACopyMoveTests, MoveAssignSelf)
{
    typename TypeParam::source_vla_type subject{{0, 1, 0, 1, 0, 1, 0, 1, 0}, TypeParam::make_source_allocator()};
    EXPECT_EQ(subject.size(), 9);
    subject = std::move(subject);
    EXPECT_EQ(subject.size(), 9);
}

#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif
