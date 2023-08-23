/// @file
/// Unit tests for cetl::VariableLengthArray type that only test compilation.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words pocma Wself

#include "cetlvast/helpers_gtest.hpp"
#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/cetlpf.hpp"

#include <memory>

template <class T>
class TestVariableLengthArrayCompiles : public testing::Test
{};

// ---------------------------------------------------------------------------------------------------------------------

using VLATypes = ::testing::Types<cetl::VariableLengthArray<int, cetl::pmr::polymorphic_allocator<int>>,
                                  cetl::VariableLengthArray<bool, cetl::pmr::polymorphic_allocator<bool>>>;

TYPED_TEST_SUITE(TestVariableLengthArrayCompiles, VLATypes, );

// ---------------------------------------------------------------------------------------------------------------------

TYPED_TEST(TestVariableLengthArrayCompiles, PolyMorphicAllocatorCompiles)
{
    static_assert(std::is_nothrow_constructible<TypeParam, const typename TypeParam::allocator_type&>::value,
                  "VariableLengthArray's default allocator must be no-throw default constructible");

    static_assert(std::is_nothrow_destructible<TypeParam>::value,
                  "VariableLengthArray's default allocator must be no-throw destructible.'.");
}

TYPED_TEST(TestVariableLengthArrayCompiles, MoveConstructorIsNoThrow)
{
    static_assert(noexcept(TypeParam(std::move(std::declval<TypeParam>()))), "Must be no-throw move constructable.");
}

// used by MoveAssignmentNoexcept test.
template <typename T, typename isaType, typename pocmaType>
struct FakeAllocator
{
    using is_always_equal                        = isaType;
    using propagate_on_container_move_assignment = pocmaType;
    using value_type                             = T;

    template <class U>
    struct rebind
    {
        typedef FakeAllocator<U, isaType, pocmaType> other;
    };
};

#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    if __GNUC__ >= 13
#        pragma GCC diagnostic ignored "-Wself-move"
#    endif
#endif

TYPED_TEST(TestVariableLengthArrayCompiles, MoveAssignmentNoexcept)
{
    using IsAlways_and_DoesProp     = FakeAllocator<typename TypeParam::value_type, std::true_type, std::true_type>;
    using IsAlways_but_DoesNotProp  = FakeAllocator<typename TypeParam::value_type, std::true_type, std::false_type>;
    using NotAlways_but_DoesProp    = FakeAllocator<typename TypeParam::value_type, std::false_type, std::true_type>;
    using NotAlways_and_DoesNotProp = FakeAllocator<typename TypeParam::value_type, std::false_type, std::false_type>;

    using VLAType_0   = cetl::VariableLengthArray<typename TypeParam::value_type, IsAlways_and_DoesProp>;
    using VLAType_0_1 = cetl::VariableLengthArray<typename TypeParam::value_type, IsAlways_and_DoesProp>;
    static_assert(noexcept(std::declval<VLAType_0>() = std::move(std::declval<VLAType_0_1>())),
                  "Violates noexcept specification for move constructor.");

    using VLAType_1   = cetl::VariableLengthArray<typename TypeParam::value_type, IsAlways_but_DoesNotProp>;
    using VLAType_1_1 = cetl::VariableLengthArray<typename TypeParam::value_type, IsAlways_but_DoesNotProp>;

    static_assert(noexcept(std::declval<VLAType_1>() = std::move(std::declval<VLAType_1_1>())),
                  "Violates noexcept specification for move constructor.");

    using VLAType_2 = cetl::VariableLengthArray<typename TypeParam::value_type, NotAlways_but_DoesProp>;
    static_assert(noexcept(std::declval<VLAType_2>() = std::move(std::declval<VLAType_2>())),
                  "Violates noexcept specification for move constructor.");

    using VLAType_3 = cetl::VariableLengthArray<typename TypeParam::value_type, NotAlways_and_DoesNotProp>;
    static_assert(!noexcept(std::declval<VLAType_3>() = std::move(std::declval<VLAType_3>())),
                  "Violates noexcept specification for move constructor.");
}

#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif

// ---------------------------------------------------------------------------------------------------------------------
