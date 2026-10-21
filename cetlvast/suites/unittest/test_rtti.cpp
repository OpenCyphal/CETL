/// @file
/// Unit tests for cetl/rtti.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/rtti.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

/// A simple non-polymorphic type that supports CETL RTTI.
struct Static final
{
    static constexpr cetl::type_id _get_type_id_() noexcept
    {
        return {0x3, 0x0};
    }
};

/// Non-polymorphic types where the derived type inherits the type ID of its secondary base,
/// so the IDs match but the base subobject is not at the start of the derived object.
struct StaticPad
{
    char pad = 'p';
};
struct StaticBase
{
    static constexpr cetl::type_id _get_type_id_() noexcept
    {
        return {0x4, 0x0};
    }
    int value = 1;
};
struct StaticDerived final : StaticPad, StaticBase
{
    int other = 2;
};

/// A simple polymorphic inheritance hierarchy: A <- B <- C.
struct PolymorphA : cetl::rtti_helper<cetl::type_id_type<0x0, 0x1>>
{
    char value = 'a';
};
struct PolymorphB : cetl::rtti_helper<cetl::type_id_type<0x1, 0x1>, PolymorphA>
{
    char value = 'b';
};
struct PolymorphC : cetl::rtti_helper<cetl::type_id_type<0x2, 0x1>, PolymorphB>
{
    char                       value = 'c';
    CETL_NODISCARD const char& value_a() const
    {
        return PolymorphA::value;
    }
    char& value_b()
    {
        return PolymorphB::value;
    }
};

/// A diamond multi-inheritance hierarchy:
///     A
///    / `
///   B   C
///    ` /
///     D
struct MultiA : cetl::rtti_helper<cetl::type_id_type<0x0, 0x2>>
{
    char value = 'a';
};
struct MultiB : cetl::rtti_helper<cetl::type_id_type<0x1, 0x2>, MultiA>
{
    char  value = 'b';
    char& value_b_a()
    {
        return MultiA::value;
    }
};
struct MultiC : cetl::rtti_helper<cetl::type_id_type<0x2, 0x2>, MultiA>
{
    char  value = 'c';
    char& value_c_a()
    {
        return MultiA::value;
    }
};
struct MultiD : cetl::rtti_helper<cetl::type_id_type<0x3, 0x2>, MultiB, MultiC>
{
    char  value = 'd';
    char& value_b()
    {
        return MultiB::value;
    }
    char& value_c()
    {
        return MultiC::value;
    }
};

TEST(test_rtti, basic)
{
    using cetl::is_instance_of;
    using cetl::rtti_cast;

    EXPECT_FALSE(is_instance_of<PolymorphB>(PolymorphA{}));
    EXPECT_TRUE(is_instance_of<PolymorphA>(PolymorphB{}));
    EXPECT_TRUE(is_instance_of<PolymorphA>(PolymorphC{}));
    EXPECT_TRUE(is_instance_of<PolymorphA>(PolymorphA{}));
    EXPECT_FALSE(is_instance_of<Static>(PolymorphA{}));

    EXPECT_EQ(nullptr, rtti_cast<Static*>(static_cast<PolymorphA*>(nullptr)));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphA*>(static_cast<PolymorphC*>(nullptr)));
    EXPECT_EQ(nullptr, rtti_cast<Static*>(static_cast<const PolymorphA*>(nullptr)));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphA*>(static_cast<const PolymorphC*>(nullptr)));
}

TEST(test_rtti, basic_single_inheritance)
{
    using cetl::rtti_cast;
    PolymorphA a;
    EXPECT_EQ('a', rtti_cast<PolymorphA*>(&a)->value);
    EXPECT_EQ('a', rtti_cast<PolymorphA*>(static_cast<const PolymorphA*>(&a))->value);
    EXPECT_EQ(&a, rtti_cast<PolymorphA*>(&a));
    EXPECT_EQ(&a, rtti_cast<PolymorphA*>(static_cast<const PolymorphA*>(&a)));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphB*>(&a));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphB*>(static_cast<const PolymorphA*>(&a)));
}

TEST(test_rtti, basic_multi_inheritance)
{
    using cetl::is_instance_of;
    MultiD d;

    EXPECT_TRUE(is_instance_of<MultiD>(d));
    EXPECT_TRUE(is_instance_of<MultiB>(d));
    EXPECT_TRUE(is_instance_of<MultiC>(d));
    EXPECT_TRUE(is_instance_of<MultiA>(d));

    EXPECT_TRUE(is_instance_of<MultiD>(static_cast<MultiB&>(d)));
    EXPECT_TRUE(is_instance_of<MultiB>(static_cast<MultiB&>(d)));
    EXPECT_TRUE(is_instance_of<MultiC>(static_cast<MultiB&>(d)));
    EXPECT_TRUE(is_instance_of<MultiA>(static_cast<MultiB&>(d)));

    EXPECT_TRUE(is_instance_of<MultiD>(static_cast<MultiC&>(d)));
    EXPECT_TRUE(is_instance_of<MultiB>(static_cast<MultiC&>(d)));
    EXPECT_TRUE(is_instance_of<MultiC>(static_cast<MultiC&>(d)));
    EXPECT_TRUE(is_instance_of<MultiA>(static_cast<MultiC&>(d)));

    EXPECT_TRUE(is_instance_of<MultiD>(static_cast<const MultiA&>(static_cast<MultiB&>(d))));
    EXPECT_TRUE(is_instance_of<MultiB>(static_cast<const MultiA&>(static_cast<MultiB&>(d))));
    EXPECT_TRUE(is_instance_of<MultiC>(static_cast<const MultiA&>(static_cast<MultiB&>(d))));
    EXPECT_TRUE(is_instance_of<MultiA>(static_cast<const MultiA&>(static_cast<MultiB&>(d))));

    EXPECT_TRUE(is_instance_of<MultiD>(static_cast<const MultiA&>(static_cast<MultiC&>(d))));
    EXPECT_TRUE(is_instance_of<MultiB>(static_cast<const MultiA&>(static_cast<MultiC&>(d))));
    EXPECT_TRUE(is_instance_of<MultiC>(static_cast<const MultiA&>(static_cast<MultiC&>(d))));
    EXPECT_TRUE(is_instance_of<MultiA>(static_cast<const MultiA&>(static_cast<MultiC&>(d))));

    MultiB b;
    EXPECT_FALSE(is_instance_of<MultiD>(b));
    EXPECT_TRUE(is_instance_of<MultiB>(b));
    EXPECT_FALSE(is_instance_of<MultiC>(b));
    EXPECT_TRUE(is_instance_of<MultiA>(b));
}

TEST(test_rtti, single_inheritance)
{
    using cetl::rtti_cast;
    using cetl::is_instance_of;

    PolymorphB b;
    PolymorphC c;

    // check values
    EXPECT_EQ('c', c.value);
    EXPECT_EQ('a', c.value_a());
    EXPECT_EQ('b', c.value_b());
    static_cast<PolymorphA&>(c).value = 'A';
    c.value_b()                       = 'B';
    EXPECT_EQ('A', c.value_a());
    EXPECT_EQ('B', static_cast<PolymorphB&>(c).value);

    // identity, b to b
    EXPECT_EQ('b', rtti_cast<PolymorphB*>(&b)->value);
    EXPECT_EQ('b', rtti_cast<PolymorphB*>(static_cast<const PolymorphB*>(&b))->value);
    EXPECT_EQ(&b, rtti_cast<PolymorphB*>(&b));
    EXPECT_EQ(&b, rtti_cast<PolymorphB*>(static_cast<const PolymorphB*>(&b)));

    // identity, c to c
    EXPECT_EQ('c', rtti_cast<PolymorphC*>(&c)->value);
    EXPECT_EQ('c', rtti_cast<PolymorphC*>(static_cast<const PolymorphC*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<PolymorphC*>(&c));
    EXPECT_EQ(&c, rtti_cast<PolymorphC*>(static_cast<const PolymorphC*>(&c)));

    // up-conversion, b to a
    EXPECT_EQ('a', rtti_cast<PolymorphA*>(&b)->value);
    EXPECT_EQ('a', rtti_cast<PolymorphA*>(static_cast<const PolymorphB*>(&b))->value);
    EXPECT_EQ(&b, rtti_cast<PolymorphA*>(&b));
    EXPECT_EQ(&b, rtti_cast<PolymorphA*>(static_cast<const PolymorphB*>(&b)));

    // up-conversion, c to b
    EXPECT_EQ('B', rtti_cast<PolymorphB*>(&c)->value);
    EXPECT_EQ('B', rtti_cast<PolymorphB*>(static_cast<const PolymorphC*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<PolymorphB*>(&c));
    EXPECT_EQ(&c, rtti_cast<PolymorphB*>(static_cast<const PolymorphC*>(&c)));

    // up-conversion, c to a
    EXPECT_EQ('A', rtti_cast<PolymorphA*>(&c)->value);
    EXPECT_EQ('A', rtti_cast<PolymorphA*>(static_cast<const PolymorphC*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<PolymorphA*>(&c));
    EXPECT_EQ(&c, rtti_cast<PolymorphA*>(static_cast<const PolymorphC*>(&c)));

    PolymorphA& a_b = b;
    PolymorphA& a_c = c;

    // down-conversion, a to b
    EXPECT_EQ('b', rtti_cast<PolymorphB*>(&a_b)->value);
    EXPECT_EQ('b', rtti_cast<PolymorphB*>(static_cast<const PolymorphA*>(&a_b))->value);
    EXPECT_EQ(&b, rtti_cast<PolymorphB*>(&a_b));
    EXPECT_EQ(&b, rtti_cast<PolymorphB*>(static_cast<const PolymorphA*>(&a_b)));

    // down-conversion, a to c
    EXPECT_EQ('c', rtti_cast<PolymorphC*>(&a_c)->value);
    EXPECT_EQ('c', rtti_cast<PolymorphC*>(static_cast<const PolymorphA*>(&a_c))->value);
    EXPECT_EQ(&c, rtti_cast<PolymorphC*>(&a_c));
    EXPECT_EQ(&c, rtti_cast<PolymorphC*>(static_cast<const PolymorphA*>(&a_c)));

    // illegal down-conversion, b to c
    EXPECT_EQ(nullptr, rtti_cast<PolymorphC*>(&a_b));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphC*>(static_cast<const PolymorphA*>(&a_b)));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphC*>(&b));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphC*>(static_cast<const PolymorphB*>(&b)));

    // is_instance_of
    EXPECT_TRUE(is_instance_of<PolymorphA>(b));
    EXPECT_TRUE(is_instance_of<PolymorphB>(b));
    EXPECT_FALSE(is_instance_of<PolymorphC>(b));
    EXPECT_FALSE(is_instance_of<Static>(b));

    EXPECT_TRUE(is_instance_of<PolymorphA>(c));
    EXPECT_TRUE(is_instance_of<PolymorphB>(c));
    EXPECT_TRUE(is_instance_of<PolymorphC>(c));
    EXPECT_FALSE(is_instance_of<Static>(c));

    EXPECT_TRUE(is_instance_of<PolymorphA>(a_b));
    EXPECT_TRUE(is_instance_of<PolymorphB>(a_b));
    EXPECT_FALSE(is_instance_of<PolymorphC>(a_b));
    EXPECT_FALSE(is_instance_of<Static>(a_b));

    EXPECT_TRUE(is_instance_of<PolymorphA>(a_c));
    EXPECT_TRUE(is_instance_of<PolymorphB>(a_c));
    EXPECT_TRUE(is_instance_of<PolymorphC>(a_c));
    EXPECT_FALSE(is_instance_of<Static>(a_c));
}

TEST(test_rtti, multi_inheritance)
{
    using cetl::rtti_cast;
    using cetl::is_instance_of;
    MultiD d;

    // check values
    EXPECT_EQ('d', d.value);
    EXPECT_EQ('b', d.value_b());
    EXPECT_EQ('c', d.value_c());
    EXPECT_EQ('a', d.value_b_a());
    EXPECT_EQ('a', d.value_c_a());
    d.value_b()   = 'B';
    d.value_c()   = 'C';
    d.value_b_a() = 'p';
    d.value_c_a() = 'o';
    EXPECT_EQ('B', static_cast<MultiB&>(d).value);
    EXPECT_EQ('C', static_cast<MultiC&>(d).value);
    EXPECT_EQ('p', static_cast<MultiA&>(static_cast<MultiB&>(d)).value);
    EXPECT_EQ('o', static_cast<MultiA&>(static_cast<MultiC&>(d)).value);

    // identity
    EXPECT_EQ('d', rtti_cast<MultiD*>(&d)->value);
    EXPECT_EQ('d', rtti_cast<MultiD*>(static_cast<const MultiD*>(&d))->value);
    EXPECT_EQ(&d, rtti_cast<MultiD*>(&d));
    EXPECT_EQ(&d, rtti_cast<MultiD*>(static_cast<const MultiD*>(&d)));

    // up-conversion, d to b
    EXPECT_EQ('B', rtti_cast<MultiB*>(&d)->value);
    EXPECT_EQ('B', rtti_cast<MultiB*>(static_cast<const MultiD*>(&d))->value);
    EXPECT_EQ(&d, rtti_cast<MultiB*>(&d));
    EXPECT_EQ(&d, rtti_cast<MultiB*>(static_cast<const MultiD*>(&d)));

    // up-conversion, d to c
    EXPECT_EQ('C', rtti_cast<MultiC*>(&d)->value);
    EXPECT_EQ('C', rtti_cast<MultiC*>(static_cast<const MultiD*>(&d))->value);
    EXPECT_EQ(&d, rtti_cast<MultiC*>(&d));
    EXPECT_EQ(&d, rtti_cast<MultiC*>(static_cast<const MultiD*>(&d)));

    // up-conversion, d to a; the base ambiguity is resolved by the order of inheritance: A<-B<-D wins over A<-C<-D.
    EXPECT_EQ('p', rtti_cast<MultiA*>(&d)->value);
    EXPECT_EQ('p', rtti_cast<MultiA*>(static_cast<const MultiD*>(&d))->value);
    EXPECT_EQ(static_cast<MultiA*>(static_cast<MultiB*>(&d)), rtti_cast<MultiA*>(&d));

    // down-conversion, b to d
    EXPECT_EQ('d', rtti_cast<MultiD*>(static_cast<MultiB*>(&d))->value);
    EXPECT_EQ(&d, rtti_cast<MultiD*>(static_cast<MultiB*>(&d)));

    // down-conversion, c to d
    EXPECT_EQ('d', rtti_cast<MultiD*>(static_cast<MultiC*>(&d))->value);
    EXPECT_EQ(&d, rtti_cast<MultiD*>(static_cast<MultiC*>(&d)));

    // down-conversion, a to d through b
    EXPECT_EQ('d', rtti_cast<MultiD*>(static_cast<MultiA*>(static_cast<MultiB*>(&d)))->value);
    EXPECT_EQ(&d, rtti_cast<MultiD*>(static_cast<MultiA*>(static_cast<MultiB*>(&d))));

    // down-conversion, a to d through c
    EXPECT_EQ('d', rtti_cast<MultiD*>(static_cast<MultiA*>(static_cast<MultiC*>(&d)))->value);
    EXPECT_EQ(&d, rtti_cast<MultiD*>(static_cast<MultiA*>(static_cast<MultiC*>(&d))));
}

/// Collects the type IDs of the given types.
template <typename... Ts>
std::vector<cetl::type_id> collect_type_ids()
{
    return {cetl::type_id_value<Ts>...};
}

TEST(test_rtti, builtin_type_ids)
{
    using cetl::has_type_id;
    using cetl::type_id;
    using cetl::type_id_value;

    static_assert(has_type_id<bool>, "");
    static_assert(has_type_id<char>, "");
    static_assert(has_type_id<signed char>, "");
    static_assert(has_type_id<unsigned char>, "");
    static_assert(has_type_id<wchar_t>, "");
    static_assert(has_type_id<char16_t>, "");
    static_assert(has_type_id<char32_t>, "");
#if defined(__cpp_char8_t)
    static_assert(has_type_id<char8_t>, "");
#endif
    static_assert(has_type_id<short>, "");
    static_assert(has_type_id<unsigned short>, "");
    static_assert(has_type_id<int>, "");
    static_assert(has_type_id<unsigned int>, "");
    static_assert(has_type_id<long>, "");
    static_assert(has_type_id<unsigned long>, "");
    static_assert(has_type_id<long long>, "");
    static_assert(has_type_id<unsigned long long>, "");
    static_assert(has_type_id<float>, "");
    static_assert(has_type_id<double>, "");
    static_assert(has_type_id<long double>, "");
    static_assert(has_type_id<std::nullptr_t>, "");
    static_assert(has_type_id<std::uint8_t>, "");
    static_assert(has_type_id<std::int64_t>, "");
    static_assert(has_type_id<Static>, "");

    static_assert(!has_type_id<void>, "");
    static_assert(!has_type_id<int*>, "");
    static_assert(!has_type_id<int[3]>, "");
    static_assert(!has_type_id<PolymorphA*>, "");

    std::vector<type_id> ids = collect_type_ids<bool,
                                                char,
                                                signed char,
                                                unsigned char,
                                                wchar_t,
                                                char16_t,
                                                char32_t,
#if defined(__cpp_char8_t)
                                                char8_t,
#endif
                                                short,
                                                unsigned short,
                                                int,
                                                unsigned int,
                                                long,
                                                unsigned long,
                                                long long,
                                                unsigned long long,
                                                float,
                                                double,
                                                long double,
                                                std::nullptr_t,
                                                Static,
                                                PolymorphA>();
    // No built-in type shares the ID reserved for void.
    EXPECT_EQ(ids.end(), std::find(ids.begin(), ids.end(), type_id_value<void>));
    // All IDs are unique.
    std::sort(ids.begin(), ids.end());
    EXPECT_EQ(ids.end(), std::adjacent_find(ids.begin(), ids.end()));
}

TEST(test_rtti, cv_and_reference_qualified_type_ids)
{
    using cetl::has_type_id;
    using cetl::type_id_value;

    static_assert(has_type_id<const int>, "");
    static_assert(has_type_id<volatile int>, "");
    static_assert(has_type_id<const volatile int>, "");
    static_assert(has_type_id<int&>, "");
    static_assert(has_type_id<const int&>, "");
    static_assert(has_type_id<int&&>, "");
    static_assert(has_type_id<const Static&>, "");

    EXPECT_EQ(type_id_value<int>, type_id_value<const int>);
    EXPECT_EQ(type_id_value<int>, type_id_value<volatile int>);
    EXPECT_EQ(type_id_value<int>, type_id_value<const volatile int>);
    EXPECT_EQ(type_id_value<int>, type_id_value<int&>);
    EXPECT_EQ(type_id_value<int>, type_id_value<const int&>);
    EXPECT_EQ(type_id_value<int>, type_id_value<int&&>);
    EXPECT_EQ(type_id_value<double>, type_id_value<const volatile double&>);
    EXPECT_EQ(type_id_value<void>, type_id_value<const void>);
    EXPECT_EQ(type_id_value<Static>, type_id_value<const Static&>);
    EXPECT_EQ(type_id_value<PolymorphA>, type_id_value<const PolymorphA>);
    EXPECT_NE(type_id_value<int>, type_id_value<unsigned int>);
}

TEST(test_rtti, non_polymorphic_is_instance_of)
{
    using cetl::is_instance_of;
    using cetl::type_id_value;

    const int    i = 42;
    const Static s{};

    EXPECT_TRUE(is_instance_of<int>(i));
    EXPECT_TRUE(is_instance_of<const int>(i));
    EXPECT_TRUE(is_instance_of<int&>(i));
    EXPECT_FALSE(is_instance_of<unsigned int>(i));
    EXPECT_FALSE(is_instance_of<long>(i));
    EXPECT_FALSE(is_instance_of<Static>(i));
    EXPECT_FALSE(is_instance_of<PolymorphA>(i));
    EXPECT_TRUE(is_instance_of(i, type_id_value<int>));
    EXPECT_FALSE(is_instance_of(i, type_id_value<void>));

    EXPECT_TRUE(is_instance_of<bool>(true));
    EXPECT_TRUE(is_instance_of<double>(1.0));
    EXPECT_FALSE(is_instance_of<float>(1.0));
    EXPECT_TRUE(is_instance_of<std::nullptr_t>(nullptr));

    EXPECT_TRUE(is_instance_of<Static>(s));
    EXPECT_TRUE(is_instance_of<const Static>(s));
    EXPECT_FALSE(is_instance_of<int>(s));
    EXPECT_FALSE(is_instance_of<PolymorphA>(s));
    EXPECT_TRUE(is_instance_of(s, type_id_value<Static>));

    // Polymorphic objects still go through _cast_().
    EXPECT_FALSE(is_instance_of<int>(PolymorphA{}));
}

TEST(test_rtti, non_polymorphic_rtti_cast)
{
    using cetl::rtti_cast;

    int    i = 42;
    Static s{};

    // identity
    EXPECT_EQ(&i, rtti_cast<int*>(&i));
    EXPECT_EQ(&i, rtti_cast<int*>(static_cast<const int*>(&i)));
    EXPECT_EQ(&i, rtti_cast<const int*>(&i));
    EXPECT_EQ(&s, rtti_cast<Static*>(&s));
    EXPECT_EQ(&s, rtti_cast<Static*>(static_cast<const Static*>(&s)));
    *rtti_cast<int*>(&i) = 7;
    EXPECT_EQ(7, i);

    // the const overload preserves constness
    static_assert(std::is_same<const int*, decltype(rtti_cast<int*>(static_cast<const int*>(&i)))>::value, "");
    static_assert(std::is_same<int*, decltype(rtti_cast<int*>(&i))>::value, "");

    // mismatched types
    EXPECT_EQ(nullptr, rtti_cast<unsigned int*>(&i));
    EXPECT_EQ(nullptr, rtti_cast<long*>(static_cast<const int*>(&i)));
    EXPECT_EQ(nullptr, rtti_cast<Static*>(&i));
    EXPECT_EQ(nullptr, rtti_cast<int*>(&s));
    EXPECT_EQ(nullptr, rtti_cast<PolymorphA*>(&s));
    EXPECT_EQ(nullptr, rtti_cast<int*>(static_cast<const Static*>(&s)));

    // null pointers
    EXPECT_EQ(nullptr, rtti_cast<int*>(static_cast<int*>(nullptr)));
    EXPECT_EQ(nullptr, rtti_cast<int*>(static_cast<const int*>(nullptr)));
    EXPECT_EQ(nullptr, rtti_cast<Static*>(static_cast<Static*>(nullptr)));

    // casting a polymorphic object to a built-in type goes through _cast_() and fails
    PolymorphA a;
    EXPECT_EQ(nullptr, rtti_cast<int*>(&a));
    EXPECT_EQ(nullptr, rtti_cast<int*>(static_cast<const PolymorphA*>(&a)));
}

TEST(test_rtti, non_polymorphic_rtti_cast_requires_identical_static_type)
{
    using cetl::rtti_cast;
    using cetl::type_id_value;

    StaticDerived d;
    StaticBase    b;
    ASSERT_EQ(type_id_value<StaticBase>, type_id_value<StaticDerived>);
    ASSERT_NE(static_cast<void*>(&d), static_cast<void*>(static_cast<StaticBase*>(&d)));

    // Equal type IDs alone must not allow a cast between different static types.
    EXPECT_EQ(nullptr, rtti_cast<StaticBase*>(&d));
    EXPECT_EQ(nullptr, rtti_cast<StaticBase*>(static_cast<const StaticDerived*>(&d)));
    EXPECT_EQ(nullptr, rtti_cast<StaticDerived*>(&b));
    EXPECT_EQ(nullptr, rtti_cast<StaticDerived*>(static_cast<const StaticBase*>(&b)));

    EXPECT_EQ(&b, rtti_cast<StaticBase*>(&b));
    EXPECT_EQ(&d, rtti_cast<StaticDerived*>(&d));
    EXPECT_EQ(&d, rtti_cast<StaticDerived*>(static_cast<const StaticDerived*>(&d)));

    // cv-qualifiers can be added but not removed.
    int          i  = 0;
    volatile int vi = 0;
    EXPECT_EQ(&i, rtti_cast<volatile int*>(&i));
    EXPECT_EQ(nullptr, rtti_cast<int*>(&vi));
    EXPECT_EQ(&vi, rtti_cast<volatile int*>(&vi));
}
