/// @file
/// Detailed introspection of allocation patterns within the
/// the VariableLengthArray type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words soccc

#include "cetl/variable_length_array.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include "cetlvast/datasets.h"
#include <memory>
#include <vector>
#include <algorithm>
#include <array>
#include <type_traits>
#include <string>

// +---------------------------------------------------------------------------+
// | TEST FIXTURES
// +---------------------------------------------------------------------------+

struct InstrumentedType
{
    static std::size_t instance_counter;
    static std::size_t total_instances_constructed;
    static std::size_t total_instances_default_constructed;
    static std::size_t total_instances_copy_constructed;
    static std::size_t total_instances_move_constructed;
    static std::size_t total_instances_implicit_int_constructed;

    InstrumentedType() noexcept
        : copy_assignments{0}
        , move_assignments{0}
        , value_{-1}
    {
        ++instance_counter;
        ++total_instances_constructed;
        ++total_instances_default_constructed;
    }

    InstrumentedType(int implicit_value) noexcept
        : copy_assignments{0}
        , move_assignments{0}
        , value_{implicit_value}
    {
        ++instance_counter;
        ++total_instances_constructed;
        ++total_instances_implicit_int_constructed;
    }

    InstrumentedType(const InstrumentedType& rhs) noexcept
        : copy_assignments{0}
        , move_assignments{0}
        , value_{rhs.value_}
    {
        ++instance_counter;
        ++total_instances_constructed;
        ++total_instances_copy_constructed;
    }

    InstrumentedType(InstrumentedType&& rhs) noexcept
        : copy_assignments{0}
        , move_assignments{0}
        , value_{rhs.value_}
    {
        rhs.value_ = -1;
        ++instance_counter;
        ++total_instances_constructed;
        ++total_instances_move_constructed;
    }

    ~InstrumentedType() noexcept
    {
        EXPECT_GT(instance_counter, 0) << "Attempted to destroy more instances than were created." << std::endl;
        --instance_counter;
    }

    InstrumentedType& operator=(InstrumentedType&& rhs) noexcept
    {
        value_     = rhs.value_;
        rhs.value_ = -1;
        ++move_assignments;
        return *this;
    }

    InstrumentedType& operator=(const InstrumentedType& rhs) noexcept
    {
        value_ = rhs.value_;
        ++copy_assignments;
        return *this;
    }

    bool operator==(const InstrumentedType& rhs) const noexcept
    {
        return (value_ == rhs.value_);
    }

    bool operator!=(const InstrumentedType& rhs) const noexcept
    {
        return !this->operator==(rhs);
    }

    operator int() const noexcept
    {
        return value_;
    }

    std::size_t            copy_assignments;
    std::size_t            move_assignments;

private:
    int value_;
};

std::size_t InstrumentedType::instance_counter                         = 0;
std::size_t InstrumentedType::total_instances_constructed              = 0;
std::size_t InstrumentedType::total_instances_default_constructed      = 0;
std::size_t InstrumentedType::total_instances_copy_constructed         = 0;
std::size_t InstrumentedType::total_instances_move_constructed         = 0;
std::size_t InstrumentedType::total_instances_implicit_int_constructed = 0;

// +---------------------------------------------------------------------------+
// | TEST SUITE
// +---------------------------------------------------------------------------+

template <typename T>
class VLADetailedAllocationTests : public ::testing::Test
{
protected:
    // +-----------------------------------------------------------------------+
    // | Test
    // +-----------------------------------------------------------------------+
    void SetUp() override
    {
        InstrumentedType::instance_counter                         = 0;
        InstrumentedType::total_instances_constructed              = 0;
        InstrumentedType::total_instances_copy_constructed         = 0;
        InstrumentedType::total_instances_move_constructed         = 0;
        InstrumentedType::total_instances_implicit_int_constructed = 0;
        InstrumentedType::total_instances_default_constructed      = 0;
        cetlvast::InstrumentedAllocatorStatistics::get().reset();
    }

    void TearDown() override
    {
        ASSERT_EQ(0, InstrumentedType::instance_counter);
        ASSERT_EQ(0, outstanding_memory());
    }

    // +-----------------------------------------------------------------------+
    // | Test Helpers
    // +-----------------------------------------------------------------------+
    using ItemT                           = typename T::value_type;
    constexpr static std::size_t ItemSize = sizeof(ItemT);

    std::size_t outstanding_memory() const
    {
        return cetlvast::InstrumentedAllocatorStatistics::get().outstanding_allocated_memory;
    }

    template <typename ContainerT>
    static std::size_t sum_memory_used_by(std::size_t add_to, const ContainerT& c)
    {
        return add_to + c.capacity() * ItemSize;
    }

    template <typename FirstContainerType, typename ...ContainerT>
    static std::size_t sum_memory_used_by(std::size_t add_to, const FirstContainerType& first, const ContainerT& ...remaining)
    {
        return add_to + sum_memory_used_by(first.capacity() * ItemSize, remaining...);
    }

    template <typename ...ContainerT>
    void account_for_all_memory(const ContainerT& ... args) const
    {
        std::size_t expected_outstanding_memory = sum_memory_used_by(0, args...);
        EXPECT_EQ(expected_outstanding_memory, this->outstanding_memory());
    }
};

// Helper for determining if the given test instantiation includes assigning
// between allocators that the test subjects will consider equal.
template <typename TypeParam>
struct AreAllocatorsEqual : public std::integral_constant<bool,
                                                          TypeParam::allocator_type::is_always_equal::value ||
                                                              TypeParam::allocator_type::is_equal::value>
{};

// Helper for distinguishing the VariableLengthArray instantiations from the std::vector instantiations used as a
// reference implementation.
template <typename TypeParam>
struct IsVariableLengthArray : public std::false_type
{};

template <typename T, typename Allocator>
struct IsVariableLengthArray<cetl::VariableLengthArray<T, Allocator>> : public std::true_type
{};

// +---------------------------------------------------------------------------+

// clang-format off
namespace cetlvast
{
using MyTypes = ::testing::Types<
    /* container type            | item type       | allocator type                                  | is_always_equal | is equal      | move prop.     | copy prop.    | */
    /*--------------------------------------------------------------------------------------------------------------------------------------------------------------------*/
/* 0*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::true_type,  std::true_type  > >,
/* 1*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::true_type,  std::true_type  > >,
/* 2*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::false_type, std::true_type  > >,
/* 3*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::false_type, std::true_type  > >,
/* 4*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::true_type,  std::true_type  > >,
/* 5*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::true_type,  std::true_type  > >,
/* 6*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::false_type, std::true_type  > >,
/* 7*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::false_type, std::true_type  > >,
/* 8*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::true_type,  std::false_type > >,
/* 9*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::true_type,  std::false_type > >,
/*10*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::false_type, std::false_type > >,
/*11*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::true_type,  std::false_type, std::false_type > >,
/*12*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::true_type,  std::false_type > >,
/*13*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::true_type,  std::false_type > >,
/*14*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::false_type, std::false_type > >,
/*15*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::true_type,  std::false_type, std::false_type > >,

/*16*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::true_type,  std::true_type  > >,
/*17*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::true_type,  std::true_type  > >,
/*18*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::false_type, std::true_type  > >,
/*19*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::false_type, std::true_type  > >,
/*20*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::true_type,  std::true_type  > >,
/*21*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::true_type,  std::true_type  > >,
/*22*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::false_type, std::true_type  > >,
/*23*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::false_type, std::true_type  > >,
/*24*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::true_type,  std::false_type > >,
/*25*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::true_type,  std::false_type > >,
/*26*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::false_type, std::false_type > >,
/*27*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::true_type,  std::false_type, std::false_type, std::false_type > >,
/*28*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::true_type,  std::false_type > >,
/*29*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::true_type,  std::false_type > >,
/*30*/ cetl::VariableLengthArray< InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::false_type, std::false_type > >,
/*31*/ std::vector<               InstrumentedType, InstrumentedNewDeleteAllocator< InstrumentedType, std::false_type, std::false_type, std::false_type, std::false_type > >
>;
}
// clang-format on

TYPED_TEST_SUITE(VLADetailedAllocationTests, cetlvast::MyTypes, );

// +---------------------------------------------------------------------------+
// | TEST CASES
// +---------------------------------------------------------------------------+

// This is a meta-test. It ensures that the test fixtures are working as expected.
TYPED_TEST(VLADetailedAllocationTests, AllocatorDefaultState)
{
    TypeParam test_subject{typename TypeParam::allocator_type()};
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_constructed);
    ASSERT_EQ(0, this->outstanding_memory());

    test_subject.emplace_back(1);
    EXPECT_EQ(1, TestFixture::ItemT::total_instances_constructed);
    ASSERT_EQ(TestFixture::ItemSize * test_subject.capacity(), this->outstanding_memory());

    ASSERT_EQ(1, test_subject.size());
    ASSERT_EQ(0, test_subject[0].move_assignments);

    ASSERT_EQ(TypeParam::allocator_type::propagate_on_container_copy_assignment::value,
              std::allocator_traits<typename TypeParam::allocator_type>::propagate_on_container_copy_assignment::value);
    ASSERT_EQ(TypeParam::allocator_type::propagate_on_container_move_assignment::value,
              std::allocator_traits<typename TypeParam::allocator_type>::propagate_on_container_move_assignment::value);
    ASSERT_EQ(TypeParam::allocator_type::is_always_equal::value,
              std::allocator_traits<typename TypeParam::allocator_type>::is_always_equal::value);
    ASSERT_FALSE(test_subject.get_allocator().was_from_soccc);

    test_subject.pop_back();
    ASSERT_EQ(0, test_subject.size());
    this->account_for_all_memory(test_subject);

    TypeParam sequence_one_to_four_a{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    TypeParam sequence_one_to_four_b{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    TypeParam sequence_six_to_nine_a{{6, 7, 8, 9}, typename TypeParam::allocator_type{}};

    ASSERT_EQ(sequence_one_to_four_a, sequence_one_to_four_b);
    ASSERT_NE(sequence_one_to_four_a, sequence_six_to_nine_a);

    std::cout << "Sizeof InstrumentedType: " << sizeof(InstrumentedType) << std::endl;
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: COPY ASSIGN
// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyAssignSameSize)
{
    TypeParam test_subject{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7, 8, 9}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(4, test_subject.size());
    EXPECT_EQ(4, test_source.size());

    test_subject = test_source;

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(4, test_source.size());
    this->account_for_all_memory(test_subject, test_source);

    // For creating the initial values from integers
    EXPECT_EQ(8, TestFixture::ItemT::total_instances_implicit_int_constructed);
    if (AreAllocatorsEqual<TypeParam>::value ||
        !TypeParam::allocator_type::propagate_on_container_copy_assignment::value)
    {
        // if the allocators are equal or the incoming allocator does not propagate
        // we expect assignments to occur only for the elements in the container
        EXPECT_EQ(8, TestFixture::ItemT::total_instances_copy_constructed);
        EXPECT_EQ(16, TestFixture::ItemT::total_instances_constructed);
    }
    else
    {
        // because the allocators are not equal the container has to completely
        // discard and reallocate its memory. This means that the container
        // will end up invoking more object constructors.
        EXPECT_EQ(12, TestFixture::ItemT::total_instances_copy_constructed);
        EXPECT_EQ(20, TestFixture::ItemT::total_instances_constructed);
    }
    // all other movement should be via copy assignment since the size of the
    // two containers is identical.
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyAssignLargeToSmall)
{
    TypeParam test_subject{{1, 2, 3, 4, 5}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7, 8, 9, 10, 11, 12}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(5, test_subject.size());
    EXPECT_EQ(7, test_source.size());

    test_subject = test_source;

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(7, test_source.size());
    this->account_for_all_memory(test_subject, test_source);

    // For creating the initial values from integers
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_implicit_int_constructed);
    // Because we are copying from a larger container to a smaller container
    // there is no different in the number of objects constructed for based on
    // allocator equality or copy propagation.
    EXPECT_EQ(19, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_move_constructed);
    EXPECT_EQ(31, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyAssignSmallToLarge)
{
    TypeParam test_subject{{1, 2, 3, 4, 5}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(5, test_subject.size());
    EXPECT_EQ(2, test_source.size());

    test_subject = test_source;

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(2, test_subject.size());
    this->account_for_all_memory(test_subject, test_source);

    // For creating the initial values from integers
    EXPECT_EQ(7, TestFixture::ItemT::total_instances_implicit_int_constructed);
    if (AreAllocatorsEqual<TypeParam>::value ||
        !TypeParam::allocator_type::propagate_on_container_copy_assignment::value)
    {
        // if the allocators are equal or the incoming allocator does not propagate
        // we expect assignments to occur only for the elements in the container
        EXPECT_EQ(7, TestFixture::ItemT::total_instances_copy_constructed);
        EXPECT_EQ(14, TestFixture::ItemT::total_instances_constructed);
    }
    else
    {
        // because the allocators are not equal the container has to completely
        // discard and reallocate its memory. This means that the container
        // will end up invoking more object constructors.
        EXPECT_EQ(7 + 2, TestFixture::ItemT::total_instances_copy_constructed);
        EXPECT_EQ(16, TestFixture::ItemT::total_instances_constructed);
    }
    // all other movement should be via copy assignment since the size of the
    // two containers is identical.
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyAssignVeryLargeToEmpty)
{
    TypeParam test_subject{typename TypeParam::allocator_type{}};
    TypeParam test_source{cetlvast::large_array_of_integers,
                          &cetlvast::large_array_of_integers[cetlvast::large_array_of_integers_size],
                          typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(0, test_subject.size());
    EXPECT_EQ(cetlvast::large_array_of_integers_size, test_source.size());

    test_subject = test_source;

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(cetlvast::large_array_of_integers_size, test_subject.size());
    this->account_for_all_memory(test_subject, test_source);
    // For creating the initial values from integers
    EXPECT_EQ(cetlvast::large_array_of_integers_size, TestFixture::ItemT::total_instances_implicit_int_constructed);
    // Because we are copying from a larger container to a smaller container
    // there is no different in the number of objects constructed for based on
    // allocator equality or copy propagation.
    EXPECT_EQ(cetlvast::large_array_of_integers_size, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_move_constructed);
    EXPECT_EQ(cetlvast::large_array_of_integers_size * 2, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyAssignFromEmpty)
{
    TypeParam test_subject{{0, 1, 2}, typename TypeParam::allocator_type{}};
    TypeParam test_source{typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(3, test_subject.size());
    EXPECT_EQ(0, test_source.size());

    test_subject = test_source;

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(0, test_subject.size());
    this->account_for_all_memory(test_subject, test_source);

    // For creating the initial values from integers
    EXPECT_EQ(3, TestFixture::ItemT::total_instances_implicit_int_constructed);
    // Because we are copying from an empty container into a full one. We don't
    // expect any additional object construction to occur.
    EXPECT_EQ(3, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_move_constructed);
    EXPECT_EQ(6, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}


// +---------------------------------------------------------------------------+
// | TEST CASES :: COPY CONSTRUCT
// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, CopyConstruct)
{
    TypeParam test_source{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    EXPECT_EQ(4, test_source.size());

    TypeParam test_subject{test_source};

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(4, test_subject.size());
    EXPECT_TRUE(test_subject.get_allocator().was_from_soccc);

    // For creating the initial values from integers and copying the source
    // array.
    EXPECT_EQ(4, TestFixture::ItemT::total_instances_implicit_int_constructed);
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(8, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}


// +---------------------------------------------------------------------------+
// | TEST CASES :: MOVE ASSIGN
// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignSameSize)
{
    TypeParam test_subject{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7, 8, 9}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    // copy the source array because we don't inspect the state of a moved
    // object.
    TypeParam copy_of_source{test_source};
    EXPECT_EQ(4, test_subject.size());
    EXPECT_EQ(4, test_source.size());

    test_subject = std::move(test_source);

    if (!TypeParam::allocator_type::propagate_on_container_move_assignment::value &&
        !AreAllocatorsEqual<TypeParam>::value)
    {
        // we didn't actually move the allocator.
        this->account_for_all_memory(test_subject, copy_of_source, test_source);
    }
    else
    {
        this->account_for_all_memory(test_subject, copy_of_source);
    }
    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(4, test_subject.size());

    // For creating the initial values from integers and copying the source
    // array.
    EXPECT_EQ(8, TestFixture::ItemT::total_instances_implicit_int_constructed);
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_copy_constructed);
    if (AreAllocatorsEqual<TypeParam>::value ||
        TypeParam::allocator_type::propagate_on_container_move_assignment::value)
    {
        // if the allocators are equal or the incoming allocator does not propagate
        // we expect assignments to occur only for the elements in the container
        EXPECT_EQ(20, TestFixture::ItemT::total_instances_constructed);
    }
    else
    {
        // because the allocators are not equal the container can't steal
        // from the rhs, however, it has enough capacity to simply move
        // everything one item at a time without additional allocations.
        // because this is assignment that means no more object constructors
        // are invoked (yes, I kept the if statements just to organize these
        // comments. It's a unittest. I can do that in a unittest).
        EXPECT_EQ(20, TestFixture::ItemT::total_instances_constructed);
    }
    // all other movement should be via move assignment since the size of the
    // two containers is identical.
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignLargeToSmall)
{
    TypeParam test_subject{{1, 2, 3, 4, 5}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7, 8, 9, 10, 11, 12}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    TypeParam copy_of_source{test_source};
    EXPECT_EQ(5, test_subject.size());
    EXPECT_EQ(7, test_source.size());

    test_subject = std::move(test_source);

    if (!TypeParam::allocator_type::propagate_on_container_move_assignment::value &&
        !AreAllocatorsEqual<TypeParam>::value)
    {
        // we didn't actually move the allocator.
        this->account_for_all_memory(test_subject, copy_of_source, test_source);
    }
    else
    {
        this->account_for_all_memory(test_subject, copy_of_source);
    }
    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(7, test_subject.size());

    // For creating the initial values from integers and for copying the source
    // container.
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_implicit_int_constructed);
    EXPECT_EQ(12 + 7, TestFixture::ItemT::total_instances_copy_constructed);

    if (!AreAllocatorsEqual<TypeParam>::value &&
        !TypeParam::allocator_type::propagate_on_container_move_assignment::value)
    {
        // Memory can't be moved. Each item will have to move instead.
        EXPECT_EQ(7, TestFixture::ItemT::total_instances_move_constructed);
        EXPECT_EQ(24 + 14, TestFixture::ItemT::total_instances_constructed);
    }
    else
    {
        // No more object construction is needed for this branch.
        EXPECT_EQ(24 + 7, TestFixture::ItemT::total_instances_constructed);
    }
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignSmallToLarge)
{
    TypeParam test_subject{{1, 2, 3, 4, 5}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7}, typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    TypeParam copy_of_source{test_source};
    EXPECT_EQ(5, test_subject.size());
    EXPECT_EQ(2, test_source.size());

    test_subject = std::move(test_source);

    if (!TypeParam::allocator_type::propagate_on_container_move_assignment::value &&
        !AreAllocatorsEqual<TypeParam>::value)
    {
         // we didn't actually move the allocator.
        this->account_for_all_memory(test_subject, copy_of_source, test_source);
    }
    else
    {
        this->account_for_all_memory(test_subject, copy_of_source);
    }
    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(2, test_subject.size());

    // For creating the initial values from integers and for copying the source
    // container.
    EXPECT_EQ(7, TestFixture::ItemT::total_instances_implicit_int_constructed);
    EXPECT_EQ(7 + 2, TestFixture::ItemT::total_instances_copy_constructed);
    // For any allocator possibility everything is via assignment so no
    // further object construction is expected.
    EXPECT_EQ(14 + 2, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignVeryLargeToEmpty)
{
    TypeParam test_subject{typename TypeParam::allocator_type{}};
    TypeParam test_source{cetlvast::large_array_of_integers,
                          &cetlvast::large_array_of_integers[cetlvast::large_array_of_integers_size],
                          typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    TypeParam copy_of_source{test_source};
    EXPECT_EQ(0, test_subject.size());
    EXPECT_EQ(cetlvast::large_array_of_integers_size, test_source.size());

    test_subject = std::move(test_source);

    if (!TypeParam::allocator_type::propagate_on_container_move_assignment::value &&
        !AreAllocatorsEqual<TypeParam>::value)
    {
         // we didn't actually move the allocator.
        this->account_for_all_memory(test_subject, copy_of_source, test_source);
    }
    else
    {
        this->account_for_all_memory(test_subject, copy_of_source);
    }
    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(cetlvast::large_array_of_integers_size, test_subject.size());

    // For creating the initial values from integers and for copying the source
    // container.
    EXPECT_EQ(cetlvast::large_array_of_integers_size, TestFixture::ItemT::total_instances_implicit_int_constructed);
    EXPECT_EQ(cetlvast::large_array_of_integers_size, TestFixture::ItemT::total_instances_copy_constructed);

    if (!AreAllocatorsEqual<TypeParam>::value &&
        !TypeParam::allocator_type::propagate_on_container_move_assignment::value)
    {
        // Memory can't be moved. Each item will have to move instead.
        EXPECT_EQ(cetlvast::large_array_of_integers_size, TestFixture::ItemT::total_instances_move_constructed);
        EXPECT_EQ(cetlvast::large_array_of_integers_size * 3, TestFixture::ItemT::total_instances_constructed);
    }
    else
    {
        // No more object construction is needed for this branch.
        EXPECT_EQ(cetlvast::large_array_of_integers_size * 2, TestFixture::ItemT::total_instances_constructed);
    }
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignFromEmpty)
{
    TypeParam test_subject{{0, 1, 2}, typename TypeParam::allocator_type{}};
    TypeParam test_source{typename TypeParam::allocator_type{}};
    this->account_for_all_memory(test_subject, test_source);
    TypeParam copy_of_source{test_source};
    EXPECT_EQ(3, test_subject.size());
    EXPECT_EQ(0, test_source.size());

    test_subject = std::move(test_source);

    if (!TypeParam::allocator_type::propagate_on_container_move_assignment::value &&
        !AreAllocatorsEqual<TypeParam>::value)
    {
         // we didn't actually move the allocator.
        this->account_for_all_memory(test_subject, copy_of_source, test_source);
    }
    else
    {
        this->account_for_all_memory(test_subject, copy_of_source);
    }
    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(0, test_subject.size());

    // For creating the initial values from integers
    EXPECT_EQ(3, TestFixture::ItemT::total_instances_implicit_int_constructed);
    EXPECT_EQ(3, TestFixture::ItemT::total_instances_copy_constructed);
    // For any allocator possibility everything is via assignment so no
    // further object construction is expected.
    EXPECT_EQ(6, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: MOVE CONSTRUCT
// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveConstruct)
{
    TypeParam test_source{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    EXPECT_EQ(4, test_source.size());
    TypeParam copy_of_source{test_source};

    TypeParam test_subject{std::move(test_source)};

    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(4, test_subject.size());
    // The allocator is move constructed from the source's allocator; no soccc is involved.
    EXPECT_FALSE(test_subject.get_allocator().was_from_soccc);

    // For creating the initial values from integers and copying the source
    // array.
    EXPECT_EQ(4, TestFixture::ItemT::total_instances_implicit_int_constructed);
    this->account_for_all_memory(test_subject, copy_of_source);
    EXPECT_TRUE(test_source.empty());
    EXPECT_EQ(8, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: ALLOCATOR-EXTENDED COPY AND MOVE CONSTRUCT
// +---------------------------------------------------------------------------+
//
// [container.alloc.reqs] specifies the allocator-extended constructors as:
//
//   X u(t, m)  - u has copies of the elements of t and u.get_allocator() == m.
//   X u(rv, m) - u has the elements rv had before the construction and
//                u.get_allocator() == m. Constant complexity if
//                m == rv.get_allocator() (the storage is adopted), otherwise
//                linear (each element is move-inserted into storage obtained
//                from m).
//
// m is used as-is: select_on_container_copy_construction is only applied by
// the non-extended copy constructor. propagate_on_container_move_assignment
// has no bearing on construction; it is only consulted by move assignment.
// These tests run against std::vector as well as VariableLengthArray so the
// expectations below are known to agree with the standard library.

TYPED_TEST(VLADetailedAllocationTests, CopyConstructWithNewAllocator)
{
    TypeParam test_source{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    EXPECT_EQ(4, test_source.size());
    const std::size_t allocations_before = cetlvast::InstrumentedAllocatorStatistics::get().allocations;

    TypeParam test_subject{test_source, typename TypeParam::allocator_type{}};

    EXPECT_EQ(test_subject, test_source);
    EXPECT_EQ(4, test_subject.size());
    EXPECT_EQ(4, test_source.size());
    // The allocator given to the constructor must be used as-is.
    EXPECT_FALSE(test_subject.get_allocator().was_from_soccc);
    this->account_for_all_memory(test_subject, test_source);

    // For creating the initial values from integers and copying them out of
    // the initializer list.
    EXPECT_EQ(4, TestFixture::ItemT::total_instances_implicit_int_constructed);
    // The elements are copied into storage obtained from the new allocator
    // regardless of allocator equality.
    EXPECT_EQ(4 + 4, TestFixture::ItemT::total_instances_copy_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_move_constructed);
    EXPECT_EQ(12, TestFixture::ItemT::total_instances_constructed);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);
    EXPECT_EQ(allocations_before + 1, cetlvast::InstrumentedAllocatorStatistics::get().allocations);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveConstructWithNewAllocator)
{
    TypeParam test_source{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    // Give the source spare capacity so that size and capacity are
    // distinguishable in the result.
    test_source.reserve(8);
    EXPECT_EQ(4, test_source.size());
    EXPECT_LE(8, test_source.capacity());
    // copy the source array because we don't inspect the state of a moved
    // object.
    TypeParam copy_of_source{test_source};
    this->account_for_all_memory(test_source, copy_of_source);

    const std::size_t move_constructed_before = TestFixture::ItemT::total_instances_move_constructed;
    const std::size_t constructed_before      = TestFixture::ItemT::total_instances_constructed;
    const std::size_t allocations_before      = cetlvast::InstrumentedAllocatorStatistics::get().allocations;

    TypeParam test_subject{std::move(test_source), typename TypeParam::allocator_type{}};

    EXPECT_EQ(test_subject, copy_of_source);
    EXPECT_EQ(4, test_subject.size());
    // The allocator given to the constructor must be used as-is.
    EXPECT_FALSE(test_subject.get_allocator().was_from_soccc);
    // Every byte of capacity reported by each container must be backed by an
    // allocation.
    this->account_for_all_memory(test_subject, copy_of_source, test_source);

    if (AreAllocatorsEqual<TypeParam>::value)
    {
        // Constant complexity: the storage is adopted so no element is touched
        // and no memory is allocated.
        EXPECT_EQ(move_constructed_before, TestFixture::ItemT::total_instances_move_constructed);
        EXPECT_EQ(constructed_before, TestFixture::ItemT::total_instances_constructed);
        EXPECT_EQ(allocations_before, cetlvast::InstrumentedAllocatorStatistics::get().allocations);
    }
    else
    {
        // Linear complexity: storage is obtained from the new allocator and
        // each element is move-inserted into it.
        EXPECT_EQ(move_constructed_before + 4, TestFixture::ItemT::total_instances_move_constructed);
        EXPECT_EQ(constructed_before + 4, TestFixture::ItemT::total_instances_constructed);
        EXPECT_EQ(allocations_before + 1, cetlvast::InstrumentedAllocatorStatistics::get().allocations);
    }
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_default_constructed);

    // The source is in a valid but unspecified state. Its allocator was not
    // moved-from so the source must remain usable.
    test_source.clear();
    EXPECT_TRUE(test_source.empty());
    test_source.push_back(5);
    EXPECT_EQ(1, test_source.size());
    EXPECT_EQ(5, static_cast<int>(test_source[0]));
    this->account_for_all_memory(test_subject, copy_of_source, test_source);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveConstructWithNewAllocatorFromEmpty)
{
    TypeParam test_source{typename TypeParam::allocator_type{}};
    // An empty container that nevertheless owns memory.
    test_source.reserve(4);
    EXPECT_EQ(0, test_source.size());
    EXPECT_LE(4, test_source.capacity());
    this->account_for_all_memory(test_source);

    TypeParam test_subject{std::move(test_source), typename TypeParam::allocator_type{}};

    EXPECT_TRUE(test_subject.empty());
    EXPECT_FALSE(test_subject.get_allocator().was_from_soccc);
    // Any capacity the new container reports must be backed by an allocation
    // made through its own allocator, otherwise the first push_back will write
    // to memory the container does not own.
    this->account_for_all_memory(test_subject, test_source);
    EXPECT_EQ(0, TestFixture::ItemT::total_instances_constructed);
}

// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveConstructWithNewAllocatorExceptionSpecification)
{
    // The non-extended move constructor adopts both the storage and the
    // allocator so it can never fail.
    static_assert(std::is_nothrow_move_constructible<TypeParam>::value, "Must be no-throw move constructible.");

    // The allocator-extended move constructor must obtain storage from m when
    // m != rv.get_allocator(). Allocation can fail so it may only be no-throw
    // when the allocators are always equal. A stronger specification turns
    // std::bad_alloc into std::terminate.
    constexpr bool is_always_equal = std::allocator_traits<typename TypeParam::allocator_type>::is_always_equal::value;
    constexpr bool is_nothrow =
        std::is_nothrow_constructible<TypeParam, TypeParam&&, const typename TypeParam::allocator_type&>::value;
    if (IsVariableLengthArray<TypeParam>::value)
    {
        // The VLA is no-throw whenever it can be.
        EXPECT_EQ(is_always_equal, is_nothrow);
    }
    else
    {
        // The standard does not specify noexcept for std::vector(vector&&, const Allocator&). libstdc++ makes it
        // conditional on is_always_equal while libc++ never makes it noexcept, so only the safety requirement holds.
        EXPECT_TRUE(is_always_equal || !is_nothrow);
    }
}

// +---------------------------------------------------------------------------+
// | TEST CASES :: ALLOCATOR REPLACEMENT
// +---------------------------------------------------------------------------+

TYPED_TEST(VLADetailedAllocationTests, MoveAssignReplacesAllocatorByMoveAssignment)
{
    TypeParam test_subject{{1, 2, 3, 4}, typename TypeParam::allocator_type{}};
    TypeParam test_source{{6, 7, 8, 9}, typename TypeParam::allocator_type{}};
    TypeParam copy_of_source{test_source};
    const std::size_t copy_assignments_before = cetlvast::InstrumentedAllocatorStatistics::get().allocator_copy_assignments;
    const std::size_t move_assignments_before = cetlvast::InstrumentedAllocatorStatistics::get().allocator_move_assignments;

    test_subject = std::move(test_source);

    EXPECT_EQ(test_subject, copy_of_source);
    // [container.alloc.reqs] "Allocator replacement is performed by copy
    // assignment, move assignment, or swapping of the allocator only if
    // propagate_on_container_copy_assignment, propagate_on_container_move_assignment,
    // or propagate_on_container_swap is true within the implementation of the
    // corresponding container operation." Move assignment of the container
    // must therefore move assign the allocator, and only when it propagates.
    EXPECT_EQ(copy_assignments_before, cetlvast::InstrumentedAllocatorStatistics::get().allocator_copy_assignments);
    if (TypeParam::allocator_type::propagate_on_container_move_assignment::value)
    {
        EXPECT_EQ(move_assignments_before + 1,
                  cetlvast::InstrumentedAllocatorStatistics::get().allocator_move_assignments);
    }
    else
    {
        EXPECT_EQ(move_assignments_before, cetlvast::InstrumentedAllocatorStatistics::get().allocator_move_assignments);
    }
}

// +---------------------------------------------------------------------------+
