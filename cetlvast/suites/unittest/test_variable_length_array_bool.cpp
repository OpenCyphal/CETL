/// @file
/// Unit tests for bool specialization of cetl::VariableLengthArray
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"

#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include <vector>

// +---------------------------------------------------------------------------+
// | TEST PROTOCOL
// +---------------------------------------------------------------------------+

template <typename AllocatorFactoryType,
          template <typename ValueType, typename AllocatorType>
          class ContainerType,
          typename AllocatorValueType>
struct TypeParamDef
{
    TypeParamDef() = delete;

    using allocator_factory = cetlvast::AllocatorTypeParamDef<AllocatorFactoryType, AllocatorValueType>;
    using container_type    = ContainerType<bool, typename allocator_factory::allocator_type>;

    template <typename... Args>
    static constexpr container_type make_bool_container(Args&&... args)
    {
        return container_type(std::forward<Args>(args)..., allocator_factory::make_allocator());
    }

    static constexpr void reset()
    {
        allocator_factory::reset();
    }
};

// +---------------------------------------------------------------------------+
// | TEST SUITE
// +---------------------------------------------------------------------------+

template <typename T>
class VLABoolTests : public ::testing::Test
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
      TypeParamDef<cetlvast::PolymorphicAllocatorNewDeleteFactory, cetl::VariableLengthArray, unsigned char>
    , TypeParamDef<cetlvast::DefaultAllocatorFactory, std::vector, unsigned char>
>;
}  // namespace cetlvast
// clang-format on

TYPED_TEST_SUITE(VLABoolTests, cetlvast::MyTypes, );

// +---------------------------------------------------------------------------+
// | TEST CASES :: Copy Construction
// +---------------------------------------------------------------------------+

TYPED_TEST(VLABoolTests, SetGetOne)
{
    auto subject = TypeParam::make_bool_container(std::initializer_list<bool>{0U, 1U});
    EXPECT_EQ(subject[0], false);
    EXPECT_EQ(subject[1], true);
}

TYPED_TEST(VLABoolTests, PushBackNine)
{
    auto subject = TypeParam::make_bool_container();
    for (unsigned char i = 0; i < 9; ++i)
    {
        subject.push_back(i % 2);
    }
    EXPECT_EQ(subject.size(), 9);
    EXPECT_GE(subject.capacity(), 16);
    EXPECT_LE(subject.capacity(), 128) << "Not sure if this is an error but it's worth checking out.";
    for (unsigned char i = 0; i < 9; ++i)
    {
        EXPECT_EQ(subject[i], i % 2);
    }
}

TYPED_TEST(VLABoolTests, TestBoolReference)
{
    auto array = TypeParam::make_bool_container();
    ASSERT_EQ(0, array.size());
    array.push_back(true);
    ASSERT_EQ(1, array.size());
    ASSERT_TRUE(array[0]);
    array.push_back(false);
    ASSERT_EQ(2, array.size());
    ASSERT_FALSE(array[1]);
    array.push_back(true);
    ASSERT_EQ(3, array.size());
    ASSERT_TRUE(array[2]);
    ASSERT_FALSE(array[1]);
    ASSERT_TRUE(array[0]);
    ASSERT_TRUE(!array[1]);
    ASSERT_FALSE(!array[0]);
    ASSERT_TRUE(array[0] == array[2]);
    ASSERT_TRUE(array[0] != array[1]);
    array[0] = array[1];
    ASSERT_FALSE(array[0]);
    ASSERT_FALSE(array[1]);
}

TYPED_TEST(VLABoolTests, TestBoolIterator)
{
    auto foo = TypeParam::make_bool_container(
        std::initializer_list<bool>{false, true, false, false, true, true, false, true, true, false});
    ASSERT_EQ(+10, (foo.end() - foo.begin()));
    ASSERT_EQ(-10, (foo.begin() - foo.end()));
    auto a = foo.begin();
    auto b = foo.begin();
    // Comparison
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    ASSERT_TRUE(a <= b);
    ASSERT_TRUE(a >= b);
    ASSERT_FALSE(a < b);
    ASSERT_FALSE(a > b);
    ++a;
    ASSERT_FALSE(a == b);
    ASSERT_TRUE(a != b);
    ASSERT_FALSE(a <= b);
    ASSERT_TRUE(a >= b);
    ASSERT_FALSE(a < b);
    ASSERT_TRUE(a > b);
    ++b;
    ASSERT_TRUE(a == b);
    ASSERT_FALSE(a != b);
    ASSERT_TRUE(a <= b);
    ASSERT_TRUE(a >= b);
    ASSERT_FALSE(a < b);
    ASSERT_FALSE(a > b);
    // Test the iterator traits
    ASSERT_TRUE((std::is_same<typename std::iterator_traits<decltype(a)>::iterator_category,
                              std::random_access_iterator_tag>::value));
    ASSERT_TRUE((std::is_same<typename std::iterator_traits<decltype(a)>::value_type, bool>::value));
    ASSERT_TRUE((std::is_same<typename std::iterator_traits<decltype(a)>::difference_type, std::ptrdiff_t>::value));
    // Test the iterator operations
    ASSERT_EQ(0, a - b);
    ASSERT_EQ(0, b - a);
    ASSERT_EQ(0, a - a);
    ASSERT_EQ(0, b - b);
    ASSERT_EQ(1, a - foo.begin());
    ASSERT_EQ(1, b - foo.begin());
    ASSERT_EQ(-1, foo.begin() - b);
    ASSERT_EQ(-1, foo.begin() - a);
    ASSERT_EQ(1, a - foo.begin());
    ASSERT_EQ(1, b - foo.begin());
    // Augmented assignment
    a += 1;
    ASSERT_EQ(1, a - b);
    ASSERT_EQ(-1, b - a);
    b -= 1;
    ASSERT_EQ(2, a - b);
    ASSERT_EQ(2, a - foo.begin());
    ASSERT_EQ(0, b - foo.begin());
    // Inc/dec
    ASSERT_EQ(2, (a++) - b);
    ASSERT_EQ(3, a - b);
    ASSERT_EQ(3, (a--) - b);
    ASSERT_EQ(2, a - b);
    ASSERT_EQ(3, (++a) - b);
    ASSERT_EQ(3, a - b);
    ASSERT_EQ(2, (--a) - b);
    ASSERT_EQ(2, a - b);
    // Add/sub
    ASSERT_EQ(4, (a + 2) - b);
    ASSERT_EQ(0, (a - 2) - b);
    // Value access
    ASSERT_EQ(2, a - foo.begin());
    ASSERT_EQ(0, b - foo.begin());
    ASSERT_EQ(false, *a);
    ASSERT_EQ(false, *b);
    ASSERT_EQ(true, a[-1]);
    ASSERT_EQ(true, b[5]);
    *a   = true;
    b[5] = false;
    ASSERT_EQ(true, *a);
    ASSERT_EQ(false, b[5]);
    // Flip bit.
    ASSERT_EQ(false, a[7]);
    ASSERT_EQ(true, foo[7]);
    a[7].flip();
    foo[7].flip();
    ASSERT_EQ(true, a[7]);
    ASSERT_EQ(false, foo[7]);
    // Check the final state.
    ASSERT_EQ(10, foo.size());
    ASSERT_LE(10, foo.capacity());
#if defined(__cpp_exceptions)
    ASSERT_EQ(false, foo.at(0));
    ASSERT_EQ(true, foo.at(1));
    ASSERT_EQ(true, foo.at(2));
    ASSERT_EQ(false, foo.at(3));
    ASSERT_EQ(true, foo.at(4));
    ASSERT_EQ(false, foo.at(5));
    ASSERT_EQ(false, foo.at(6));
    ASSERT_EQ(false, foo.at(7));
    ASSERT_EQ(true, foo.at(8));
    ASSERT_EQ(true, foo.at(9));
#else
    ASSERT_EQ(false, foo[0]);
    ASSERT_EQ(true, foo[1]);
    ASSERT_EQ(true, foo[2]);
    ASSERT_EQ(false, foo[3]);
    ASSERT_EQ(true, foo[4]);
    ASSERT_EQ(false, foo[5]);
    ASSERT_EQ(false, foo[6]);
    ASSERT_EQ(false, foo[7]);
    ASSERT_EQ(true, foo[8]);
    ASSERT_EQ(true, foo[9]);
#endif
    // Constant iterators.
    ASSERT_EQ(false, *foo.cbegin());
    ASSERT_EQ(true, *(foo.cend() - 1));
    ASSERT_EQ(true, foo.cbegin()[2]);
}
