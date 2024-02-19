/// @file
/// Unit tests for cetl/rtti.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/rtti.hpp>
#include <gtest/gtest.h>

#define CETL_RTTI(base, ...)                                                          \
public:                                                                               \
    static constexpr cetl::type_id _type_id_{__VA_ARGS__};                            \
                                                                                      \
protected:                                                                            \
    cetl::type_id _get_type_id_() const noexcept override                             \
    {                                                                                 \
        return _type_id_;                                                             \
    }                                                                                 \
    void* _cast_(const cetl::type_id& id) noexcept override                           \
    {                                                                                 \
        return (id == _type_id_) ? static_cast<void*>(this) : base::_cast_(id);       \
    }                                                                                 \
    const void* _cast_(const cetl::type_id& id) const noexcept override               \
    {                                                                                 \
        return (id == _type_id_) ? static_cast<const void*>(this) : base::_cast_(id); \
    }                                                                                 \
                                                                                      \
private:

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
    static constexpr cetl::type_id _type_id_{
        {0x3, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}};
    const char value = 'd';
};

TEST(test_rtti, basic)
{
    A a;
    EXPECT_EQ(cetl::get_type_id(a),
              cetl::type_id({0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF}));
}
