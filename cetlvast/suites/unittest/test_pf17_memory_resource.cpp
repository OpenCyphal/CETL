/// @file
/// Unit tests for memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"

using ::testing::Return;
using ::testing::Sequence;
using ::testing::Ref;
using ::testing::_;
using ::testing::NiceMock;

// +----------------------------------------------------------------------+
// | Test Suite :: TestMemoryResourceABC
// +----------------------------------------------------------------------+
/**
 * Test suite for testing the Abstract Base Class, cetl::pf17::pmr::memory_resource
 * against std::pmr::memory_resource.
 */
template <typename T>
class TestMemoryResourceABC : public ::testing::Test
{};

// clang-format off
using MemoryResourceMocks = ::testing::Types<
      cetlvast::MockPf17MemoryResource
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    , cetlvast::MockStdMemoryResource
#endif
>;
// clang-format on

TYPED_TEST_SUITE(TestMemoryResourceABC, MemoryResourceMocks, );

// +----------------------------------------------------------------------+

TYPED_TEST(TestMemoryResourceABC, TestAllocation)
{
    TypeParam subject;
    void*     stack_ptr = &subject;

    Sequence s1;
    EXPECT_CALL(subject, do_allocate(56, alignof(std::max_align_t)))
        .Times(1)
        .InSequence(s1)
        .WillOnce(Return(stack_ptr));

    void* allocated = subject.allocate(56);
    ASSERT_EQ(stack_ptr, allocated);

    EXPECT_CALL(subject, do_deallocate(allocated, 56, alignof(std::max_align_t))).Times(1).InSequence(s1);

    subject.deallocate(allocated, 56);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestMemoryResourceABC, TestMemberEquality)
{
    TypeParam subject;

    EXPECT_CALL(subject, do_is_equal(Ref(subject))).Times(1).WillOnce(Return(true));

    ASSERT_TRUE(subject.is_equal(subject));
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestMemoryResourceABC, TestGlobalEquality)
{
    NiceMock<TypeParam> subject0;
    TypeParam subject1;

    ON_CALL(subject0, do_is_equal(Ref(subject0))).WillByDefault(Return(true));
    ON_CALL(subject0, do_is_equal(_)).WillByDefault(Return(false));

    ASSERT_FALSE(subject0 == subject1);
    ASSERT_TRUE(subject0 == subject0);

    ASSERT_TRUE(subject0 != subject1);
    ASSERT_FALSE(subject0 != subject0);
}


// +----------------------------------------------------------------------+


TYPED_TEST(TestMemoryResourceABC, TestNullMemoryResourceAllocation)
{
    auto* subject = TypeParam::get();
    ASSERT_NE(nullptr, subject);
    int dummy = 1;
    void* dummy_memory = &dummy;
#if defined(__cpp_exceptions)
    EXPECT_THROW((void)subject->allocate(1), std::bad_alloc);
#else
    if(TypeParam::ReturnsNullWhenFNoExceptions)
    {
        EXPECT_EQ(nullptr, subject->allocate(1));
    }
    // else the behavior is undefined and cannot be tested.
#endif

    // nothing should happen.
    subject->deallocate(dummy_memory, sizeof(dummy));
    ASSERT_EQ(1, dummy);
}

// +----------------------------------------------------------------------+


TYPED_TEST(TestMemoryResourceABC, TestNullMemoryResourceMemberEquality)
{
    ASSERT_TRUE(TypeParam::get()->is_equal(*TypeParam::get()));

    TypeParam fixture;

    ASSERT_FALSE(TypeParam::get()->is_equal(fixture));

}
