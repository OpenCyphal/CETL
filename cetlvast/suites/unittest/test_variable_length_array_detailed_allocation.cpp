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
