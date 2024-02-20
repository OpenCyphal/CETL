/// @file
/// Unit tests for cetl/rtti.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/rtti.hpp>
#include <gtest/gtest.h>

/// An optional helper that can be used to implement CETL RTTI support with minimal boilerplate.
/// Use it in the public section of the class definition.
///
/// The first argument is the base class of the class that is being defined, which can be \c cetl::rtti.
///
/// The following arguments are the 16 bytes of the type identifier exposed via \c _get_static_type_id_;
/// if less than 16 bytes are provided, the remaining bytes are zeroed.
#define CETL_RTTI(base, ...)                                                                       \
    static constexpr cetl::type_id _get_static_type_id_() noexcept                                 \
    {                                                                                              \
        return cetl::type_id{__VA_ARGS__};                                                         \
    }                                                                                              \
    cetl::type_id _get_polymorphic_type_id_() const noexcept override                              \
    {                                                                                              \
        return _get_static_type_id_();                                                             \
    }                                                                                              \
    void* _cast_(const cetl::type_id& id)& noexcept override                                       \
    {                                                                                              \
        return (id == _get_static_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);       \
    }                                                                                              \
    const void* _cast_(const cetl::type_id& id) const& noexcept override                           \
    {                                                                                              \
        return (id == _get_static_type_id_()) ? static_cast<const void*>(this) : base::_cast_(id); \
    }

struct A : cetl::rtti
{
    ~A() override    = default;
    const char value = 'a';
    CETL_RTTI(cetl::rtti, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF)
};

struct B : A
{
    const char value = 'b';
    CETL_RTTI(A, 0x1, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF)
};

struct C : B
{
    const char value = 'c';
    CETL_RTTI(B, 0x2, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF)
};

struct D final
{
    static constexpr cetl::type_id _get_static_type_id_() noexcept
    {
        return {0x3, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF};
    }
};

TEST(test_rtti, basic)
{
    EXPECT_EQ(cetl::get_type_id(A{}), A::_get_static_type_id_());
    EXPECT_EQ(cetl::get_type_id(B{}), B::_get_static_type_id_());
    EXPECT_EQ(cetl::get_type_id(C{}), C::_get_static_type_id_());
    EXPECT_EQ(cetl::get_type_id(D{}), D::_get_static_type_id_());

    EXPECT_EQ(nullptr, cetl::rtti_cast<D*>(static_cast<A*>(nullptr)));
    EXPECT_EQ(nullptr, cetl::rtti_cast<A*>(static_cast<C*>(nullptr)));
    EXPECT_EQ(nullptr, cetl::rtti_cast<D*>(static_cast<const A*>(nullptr)));
    EXPECT_EQ(nullptr, cetl::rtti_cast<A*>(static_cast<const C*>(nullptr)));

    A a;
    EXPECT_EQ(cetl::get_type_id(a),
              cetl::type_id({0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}));
    EXPECT_EQ(cetl::get_type_id(a), a._get_static_type_id_());
    EXPECT_EQ('a', cetl::rtti_cast<A*>(&a)->value);
    EXPECT_EQ('a', cetl::rtti_cast<A*>(static_cast<const A*>(&a))->value);
    EXPECT_EQ(&a, cetl::rtti_cast<A*>(&a));
    EXPECT_EQ(&a, cetl::rtti_cast<A*>(static_cast<const A*>(&a)));
    EXPECT_EQ(nullptr, cetl::rtti_cast<B*>(&a));
    EXPECT_EQ(nullptr, cetl::rtti_cast<B*>(static_cast<const A*>(&a)));
}

TEST(test_rtti, polymorphism)
{
    using cetl::rtti_cast;
    using cetl::is_instance_of;

    B b;
    C c;

    EXPECT_EQ(cetl::get_type_id(b),
              cetl::type_id({0x1, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}));
    EXPECT_EQ(cetl::get_type_id(c),
              cetl::type_id({0x2, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}));
    EXPECT_EQ(cetl::get_type_id(b), b._get_static_type_id_());
    EXPECT_EQ(cetl::get_type_id(c), c._get_static_type_id_());

    // identity, b to b
    EXPECT_EQ('b', rtti_cast<B*>(&b)->value);
    EXPECT_EQ('b', rtti_cast<B*>(static_cast<const B*>(&b))->value);
    EXPECT_EQ(&b, rtti_cast<B*>(&b));
    EXPECT_EQ(&b, rtti_cast<B*>(static_cast<const B*>(&b)));

    // identity, c to c
    EXPECT_EQ('c', rtti_cast<C*>(&c)->value);
    EXPECT_EQ('c', rtti_cast<C*>(static_cast<const C*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<C*>(&c));
    EXPECT_EQ(&c, rtti_cast<C*>(static_cast<const C*>(&c)));

    // up-conversion, b to a
    EXPECT_EQ('a', rtti_cast<A*>(&b)->value);
    EXPECT_EQ('a', rtti_cast<A*>(static_cast<const B*>(&b))->value);
    EXPECT_EQ(&b, rtti_cast<A*>(&b));
    EXPECT_EQ(&b, rtti_cast<A*>(static_cast<const B*>(&b)));

    // up-conversion, c to b
    EXPECT_EQ('b', rtti_cast<B*>(&c)->value);
    EXPECT_EQ('b', rtti_cast<B*>(static_cast<const C*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<B*>(&c));
    EXPECT_EQ(&c, rtti_cast<B*>(static_cast<const C*>(&c)));

    // up-conversion, c to a
    EXPECT_EQ('a', rtti_cast<A*>(&c)->value);
    EXPECT_EQ('a', rtti_cast<A*>(static_cast<const C*>(&c))->value);
    EXPECT_EQ(&c, rtti_cast<A*>(&c));
    EXPECT_EQ(&c, rtti_cast<A*>(static_cast<const C*>(&c)));

    A& a_b = b;
    A& a_c = c;

    // down-conversion, a to b
    EXPECT_EQ('b', rtti_cast<B*>(&a_b)->value);
    EXPECT_EQ('b', rtti_cast<B*>(static_cast<const A*>(&a_b))->value);
    EXPECT_EQ(&b, rtti_cast<B*>(&a_b));
    EXPECT_EQ(&b, rtti_cast<B*>(static_cast<const A*>(&a_b)));

    // down-conversion, a to c
    EXPECT_EQ('c', rtti_cast<C*>(&a_c)->value);
    EXPECT_EQ('c', rtti_cast<C*>(static_cast<const A*>(&a_c))->value);
    EXPECT_EQ(&c, rtti_cast<C*>(&a_c));
    EXPECT_EQ(&c, rtti_cast<C*>(static_cast<const A*>(&a_c)));

    // illegal down-conversion, b to c
    EXPECT_EQ(nullptr, rtti_cast<C*>(&a_b));
    EXPECT_EQ(nullptr, rtti_cast<C*>(static_cast<const A*>(&a_b)));
    EXPECT_EQ(nullptr, rtti_cast<C*>(&b));
    EXPECT_EQ(nullptr, rtti_cast<C*>(static_cast<const B*>(&b)));

    // is_instance_of
    EXPECT_TRUE(is_instance_of<A>(b));
    EXPECT_TRUE(is_instance_of<B>(b));
    EXPECT_FALSE(is_instance_of<C>(b));
    EXPECT_FALSE(is_instance_of<D>(b));

    EXPECT_TRUE(is_instance_of<A>(c));
    EXPECT_TRUE(is_instance_of<B>(c));
    EXPECT_TRUE(is_instance_of<C>(c));
    EXPECT_FALSE(is_instance_of<D>(c));

    EXPECT_TRUE(is_instance_of<A>(a_b));
    EXPECT_TRUE(is_instance_of<B>(a_b));
    EXPECT_FALSE(is_instance_of<C>(a_b));
    EXPECT_FALSE(is_instance_of<D>(a_b));

    EXPECT_TRUE(is_instance_of<A>(a_c));
    EXPECT_TRUE(is_instance_of<B>(a_c));
    EXPECT_TRUE(is_instance_of<C>(a_c));
    EXPECT_FALSE(is_instance_of<D>(a_c));
}
