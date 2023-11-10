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

template <typename T>
class VLABoolTestsVLAOnly : public ::testing::Test
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
using VLAAndVectorTypes = ::testing::Types<
      TypeParamDef<cetlvast::PolymorphicAllocatorNewDeleteFactory, cetl::VariableLengthArray, unsigned char>
    , TypeParamDef<cetlvast::DefaultAllocatorFactory, std::vector, unsigned char>
>;
using VLATypes = ::testing::Types<
      TypeParamDef<cetlvast::PolymorphicAllocatorNewDeleteFactory, cetl::VariableLengthArray, unsigned char>
>;
}  // namespace cetlvast
// clang-format on

TYPED_TEST_SUITE(VLABoolTests, cetlvast::VLAAndVectorTypes, );
TYPED_TEST_SUITE(VLABoolTestsVLAOnly, cetlvast::VLATypes, );

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

    typename decltype(foo)::const_iterator const_begin_iterator =
        const_cast<typename std::add_pointer<typename std::add_const<decltype(foo)>::type>::type>(&foo)->begin();
    typename decltype(foo)::const_iterator const_end_interator =
        const_cast<typename std::add_pointer<typename std::add_const<decltype(foo)>::type>::type>(&foo)->end();

    ASSERT_EQ(false, *const_begin_iterator);
    ASSERT_EQ(true, *(const_end_interator - 1));
    ASSERT_EQ(true, const_begin_iterator[2]);
}

TYPED_TEST(VLABoolTests, TestBoolPopBack)
{
    auto test_subject = TypeParam::make_bool_container(
        std::initializer_list<bool>{true, false, true, false, true, false, true, false, true});
    std::size_t starting_size = test_subject.size();
    ASSERT_EQ(9, starting_size);
    while (starting_size > 0)
    {
        ASSERT_EQ((starting_size % 2), test_subject[starting_size - 1]);
        test_subject.pop_back();
        --starting_size;
        ASSERT_EQ(starting_size, test_subject.size());
    }
}

TYPED_TEST(VLABoolTests, TestBoolResize)
{
    auto array = TypeParam::make_bool_container();
    ASSERT_EQ(0, array.size());
    for (std::size_t i = 1; i <= 64; ++i)
    {
        array.resize(i, false);
        ASSERT_EQ(i, array.size());
        ASSERT_EQ(false, array[i - 1]);
    }
}

TYPED_TEST(VLABoolTests, TestBoolResizeToZero)
{
    auto test_subject = TypeParam::make_bool_container(
        std::initializer_list<bool>{true, false, true, false, true, false, true, false, true});
    ASSERT_EQ(9, test_subject.size());
    test_subject.resize(0);
    ASSERT_EQ(0, test_subject.size());
    ASSERT_LE(9, test_subject.capacity());
}

TYPED_TEST(VLABoolTests, TestBoolResizeWithDefault)
{
    auto array = TypeParam::make_bool_container(std::initializer_list<bool>{false});
    array.resize(22, true);
    ASSERT_EQ(22, array.size());
    ASSERT_EQ(false, array[0]);
    for (std::size_t i = 1; i < array.size(); ++i)
    {
        ASSERT_EQ(true, array[i]);
    }
    // Remember that, if we resize down, the default argument is not used for anything.
    array.resize(9, false);
    ASSERT_EQ(9, array.size());
    ASSERT_EQ(false, array[0]);
    for (std::size_t i = 1; i < array.size(); ++i)
    {
        ASSERT_EQ(true, array[i]);
    }
}

TYPED_TEST(VLABoolTests, TestBoolResizeOneBit)
{
    auto array = TypeParam::make_bool_container(std::initializer_list<bool>{true, false, true});
    ASSERT_EQ(3, array.size());
    ASSERT_EQ(1, array[0]);
    ASSERT_EQ(0, array[1]);
    ASSERT_EQ(1, array[2]);
    array.resize(4, 1);
    ASSERT_EQ(4, array.size());
    ASSERT_EQ(1, array[0]);
    ASSERT_EQ(0, array[1]);
    ASSERT_EQ(1, array[2]);
    ASSERT_EQ(1, array[3]);
    array.resize(5);
    ASSERT_EQ(5, array.size());
    ASSERT_EQ(1, array[0]);
    ASSERT_EQ(0, array[1]);
    ASSERT_EQ(1, array[2]);
    ASSERT_EQ(1, array[3]);
    ASSERT_EQ(0, array[4]);
}

TYPED_TEST(VLABoolTestsVLAOnly, TestBoolExceedingMaxSizeMax)
{
    std::size_t max_size_max = 1ul;
    auto array = TypeParam::make_bool_container(1ul);
    array.push_back(true);
    ASSERT_EQ(1, array.size());

    // Test resize()
#ifdef __cpp_exceptions
    ASSERT_THROW(array.resize(2 * max_size_max), std::length_error);
#else
    array.resize(2 * max_size_max);
    ASSERT_EQ(max_size_max, array.size());
#endif  // __cpp_exceptions

    // Test push_back()
#ifdef __cpp_exceptions
    ASSERT_THROW(array.push_back(true), std::length_error);
#else
    array.push_back(true);
    ASSERT_EQ(max_size_max, array.size());
#endif  // __cpp_exceptions

    // Test emplace_back()
#ifdef __cpp_exceptions
    ASSERT_THROW(array.emplace_back(true), std::length_error);
#else
    array.emplace_back(true);
    ASSERT_EQ(max_size_max, array.size());
#endif  // __cpp_exceptions
}

TYPED_TEST(VLABoolTests, TestBoolFront)
{
    auto array = TypeParam::make_bool_container(std::initializer_list<bool>{true, false, true});
    ASSERT_TRUE(array.front());

    typename decltype(array)::const_reference value =
        const_cast<typename std::add_pointer<typename std::add_const<decltype(array)>::type>::type>(&array)->front();
    ASSERT_TRUE(value);
}

TYPED_TEST(VLABoolTests, TestBoolBack)
{
    auto array = TypeParam::make_bool_container(std::initializer_list<bool>{true, false, true});
    ASSERT_TRUE(array.back());

    typename decltype(array)::const_reference value =
        const_cast<typename std::add_pointer<typename std::add_const<decltype(array)>::type>::type>(&array)->back();
    ASSERT_TRUE(value);
}

TYPED_TEST(VLABoolTests, TestAssignCountAndValue)
{
    auto array = TypeParam::make_bool_container();
    array.assign(0, true);
    ASSERT_EQ(0, array.size());
    array.assign(1, false);
    ASSERT_EQ(1, array.size());
    ASSERT_FALSE(array[0]);
    array.resize(9, false);
    ASSERT_EQ(9, array.size());
    array.assign(3, false);
    ASSERT_EQ(3, array.size());
    for(auto i = array.begin(), e = array.end(); i != e; ++i)
    {
        ASSERT_FALSE(*i);
    }
    array.assign(17, true);
    ASSERT_EQ(17, array.size());
    for(auto i = array.begin(), e = array.end(); i != e; ++i)
    {
        ASSERT_TRUE(*i);
    }
}

TYPED_TEST(VLABoolTestsVLAOnly, ConstructFromIteratorRange)
{
    // Provide it too much stuff
    std::vector<bool> data{false, true, false};

#if defined(__cpp_exceptions)
    EXPECT_THROW((void)TypeParam::make_bool_container(data.begin(), data.end(), 2U), std::length_error);
#elif (__cplusplus == CETL_CPP_STANDARD_14)
    auto subject = TypeParam::make_bool_container(data.begin(), data.end(), 2U);
    // Should only add to max
    EXPECT_EQ(subject.size(), 2);
    EXPECT_EQ(subject[0], false);
    EXPECT_EQ(subject[1], true);
#else
    GTEST_SKIP() << "C++17 pmr does not support defined out of memory behaviour without exceptions.";
#endif
}

TYPED_TEST(VLABoolTestsVLAOnly, ExceedMaxSizeMaxFails)
{
    std::size_t max = 3U;
    auto subject = TypeParam::make_bool_container(max);
    for (std::size_t i = 0; i < max; i++) {
        subject.push_back(true);
    }
#if defined(__cpp_exceptions)
    EXPECT_THROW((void)subject.push_back(true), std::length_error);
#elif (__cplusplus == CETL_CPP_STANDARD_14)
    // Try to add one too many
    subject.push_back(true);
    // Shouldn't have been added
    EXPECT_EQ(subject.size(), 3);
    EXPECT_EQ(subject[0], true);
    EXPECT_EQ(subject[1], true);
    EXPECT_EQ(subject[2], true);
#else
    GTEST_SKIP() << "C++17 pmr does not support defined out of memory behaviour without exceptions.";
#endif
}
