/// @file
/// Unit tests for cetl/any.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/any.hpp>

#include <complex>
#include <functional>
#include <string>
#include <gtest/gtest.h>

// NOLINTBEGIN(*-use-after-move)

namespace
{

using cetl::any;
using cetl::any_cast;
using cetl::make_any;
using cetl::in_place_type_t;
using cetl::type_id;
using cetl::type_id_type;
using cetl::rtti_helper;

using namespace std::string_literals;

/// HELPERS ---------------------------------------------------------------------------------------------------------

#if defined(__cpp_exceptions)

// Workaround for GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
// Should be used in the tests where exceptions are expected (see `EXPECT_THROW`).
const auto sink = [](auto&&) {};

#endif

enum class side_effect_op : char
{
    Construct     = '@',
    CopyConstruct = 'C',
    MoveConstruct = 'M',
    CopyAssign    = '=',
    MoveAssign    = '<',
    Destruct      = '~',
    DestructMoved = '_',
};
using side_effect_fn = std::function<void(side_effect_op)>;

struct side_effect_stats
{
    std::string ops;
    int         assignments = 0;
    int         constructs  = 0;
    int         destructs   = 0;

    auto make_side_effect_fn()
    {
        return [this](side_effect_op op) {
            ops += static_cast<char>(op);

            constructs += (op == side_effect_op::Construct) ? 1 : 0;
            constructs += (op == side_effect_op::CopyConstruct) ? 1 : 0;
            constructs += (op == side_effect_op::MoveConstruct) ? 1 : 0;

            assignments += (op == side_effect_op::CopyAssign) ? 1 : 0;
            assignments += (op == side_effect_op::MoveAssign) ? 1 : 0;

            destructs += (op == side_effect_op::Destruct) ? 1 : 0;
            destructs += (op == side_effect_op::DestructMoved) ? 1 : 0;
        };
    }
};

struct TestBase : rtti_helper<type_id_type<0x0>>
{
    char payload_;
    int  value_ = 0;
    bool moved_ = false;

    TestBase(const char payload, side_effect_fn side_effect)
        : payload_(payload)
        , side_effect_(std::move(side_effect))
    {
        side_effect_(side_effect_op::Construct);
    }
    TestBase(const TestBase& other)
    {
        copy_from(other, side_effect_op::CopyConstruct);
    }
    TestBase(TestBase&& other) noexcept
    {
        move_from(other, side_effect_op::MoveConstruct);
    }

    ~TestBase() override
    {
        side_effect_(moved_ ? side_effect_op::DestructMoved : side_effect_op::Destruct);
    }

    TestBase& operator=(const TestBase& other)
    {
        copy_from(other, side_effect_op::CopyAssign);
        return *this;
    }

    TestBase& operator=(TestBase&& other) noexcept
    {
        move_from(other, side_effect_op::MoveAssign);
        return *this;
    }

    CETL_NODISCARD virtual const char* what() const noexcept
    {
        return "TestBase";
    }

private:
    side_effect_fn side_effect_;

    void copy_from(const TestBase& other, const side_effect_op op)
    {
        payload_     = other.payload_;
        side_effect_ = other.side_effect_;
        value_       = other.value_ + 10;

        side_effect_(op);
    }

    void move_from(TestBase& other, const side_effect_op op)
    {
        payload_     = other.payload_;
        side_effect_ = other.side_effect_;
        value_       = other.value_ + 1;

        other.moved_   = true;
        other.payload_ = '\0';

        side_effect_(op);
    }

};  // TestBase

struct TestCopyableOnly final : TestBase
{
    explicit TestCopyableOnly(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : TestBase(payload, std::move(side_effect))
    {
    }
    TestCopyableOnly(const TestCopyableOnly& other)     = default;
    TestCopyableOnly(TestCopyableOnly&& other) noexcept = delete;

    TestCopyableOnly& operator=(const TestCopyableOnly& other)     = default;
    TestCopyableOnly& operator=(TestCopyableOnly&& other) noexcept = delete;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "TestCopyableOnly";
    }

    // rtti

    static constexpr type_id _get_type_id_() noexcept
    {
        return {0x0, 0b01};
    }

    CETL_NODISCARD void* _cast_(const type_id& id) & noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    }
    CETL_NODISCARD const void* _cast_(const type_id& id) const& noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<const void*>(this) : base::_cast_(id);
    }

private:
    using base = TestBase;
};

struct TestMovableOnly final : TestBase
{
    explicit TestMovableOnly(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : TestBase(payload, std::move(side_effect))
    {
    }
    TestMovableOnly(const TestMovableOnly& other)     = delete;
    TestMovableOnly(TestMovableOnly&& other) noexcept = default;

    TestMovableOnly& operator=(const TestMovableOnly& other)     = delete;
    TestMovableOnly& operator=(TestMovableOnly&& other) noexcept = default;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "TestMovableOnly";
    }

    // rtti

    static constexpr type_id _get_type_id_() noexcept
    {
        return {0x0, 0b10};
    }

    CETL_NODISCARD void* _cast_(const type_id& id) & noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    }
    CETL_NODISCARD const void* _cast_(const type_id& id) const& noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<const void*>(this) : base::_cast_(id);
    }

private:
    using base = TestBase;
};

struct TestCopyableAndMovable final : TestBase
{
    // Just to make this class a bit bigger than base.
    char place_holder_;

    explicit TestCopyableAndMovable(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : TestBase(payload, std::move(side_effect))
        , place_holder_{payload}
    {
    }

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "TestCopyableAndMovable";
    }

    // rtti

    static constexpr type_id _get_type_id_() noexcept
    {
        return {0x0, 0b11};
    }

    CETL_NODISCARD void* _cast_(const type_id& id) & noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    }
    CETL_NODISCARD const void* _cast_(const type_id& id) const& noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<const void*>(this) : base::_cast_(id);
    }

private:
    using base = TestBase;
};

/// TESTS -----------------------------------------------------------------------------------------------------------

TEST(test_any, cppref_example)
{
    using any = any<std::max(sizeof(int), sizeof(double))>;

    any a = 1;
    EXPECT_EQ(1, any_cast<int>(a));

    a = 3.14;
    EXPECT_EQ(3.14, any_cast<double>(a));

    a = true;
    EXPECT_TRUE(any_cast<bool>(a));

    // bad cast
    a = 1;
#if defined(__cpp_exceptions)
    EXPECT_THROW(sink(any_cast<float>(a)), cetl::bad_any_cast);
#else
    EXPECT_EQ(nullptr, any_cast<float>(&a));
#endif

    a = 2;
    EXPECT_TRUE(a.has_value());

    // reset
    a.reset();
    EXPECT_FALSE(a.has_value());

    // pointer to contained data
    a = 3;
    EXPECT_EQ(3, *any_cast<int>(&a));
}

TEST(test_any, ctor_1_default)
{
    EXPECT_FALSE((any<0>{}.has_value()));
    EXPECT_FALSE((any<0, false>{}.has_value()));
    EXPECT_FALSE((any<0, false, true>{}.has_value()));
    EXPECT_FALSE((any<0, true, false>{}.has_value()));
    EXPECT_FALSE((any<0, true, true, 1>{}.has_value()));

    EXPECT_FALSE((any<1>{}.has_value()));
    EXPECT_FALSE((any<1, false>{}.has_value()));
    EXPECT_FALSE((any<1, false, true>{}.has_value()));
    EXPECT_FALSE((any<1, true, false>{}.has_value()));
    EXPECT_FALSE((any<1, true, true, 1>{}.has_value()));

    EXPECT_FALSE((any<13>{}.has_value()));
    EXPECT_FALSE((any<13, false>{}.has_value()));
    EXPECT_FALSE((any<13, false, true>{}.has_value()));
    EXPECT_FALSE((any<13, true, false>{}.has_value()));
    EXPECT_FALSE((any<13, true, true, 1>{}.has_value()));
}

TEST(test_any, ctor_2_copy)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        const uut src{42};
        uut       dst{src};

        EXPECT_EQ(42, any_cast<int>(src));
        EXPECT_EQ(42, any_cast<int>(dst));
    }

    // Copyable and Movable `any`
    {
        using test = TestCopyableAndMovable;
        using uut  = any<sizeof(test)>;

        const uut src{test{}};
        uut       dst{src};

        EXPECT_EQ(1 + 10, any_cast<test>(src).value_);
        EXPECT_EQ(1, any_cast<const test&>(src).value_);

        EXPECT_EQ(1 + 10 + 10, any_cast<test>(dst).value_);
        EXPECT_EQ(1 + 10, any_cast<test&>(dst).value_);
        EXPECT_EQ(1 + 10, any_cast<const test&>(dst).value_);

        EXPECT_FALSE(any_cast<const test&>(dst).moved_);
        EXPECT_EQ(1 + 10 + 1, any_cast<test>(std::move(dst)).value_);
        EXPECT_TRUE(any_cast<const test&>(dst).moved_);
    }

    // Copyable only `any`
    {
        using test = TestCopyableOnly;
        using uut  = any<sizeof(test), true, false>;

        const test value{};
        uut        src{value};
        const uut  dst{src};

        EXPECT_EQ(10, any_cast<test&>(src).value_);
        EXPECT_EQ(10, any_cast<const test&>(src).value_);

        EXPECT_EQ(10 + 10, any_cast<const test&>(dst).value_);
    }

    // Movable only `any`
    {
        using test = TestMovableOnly;
        using uut  = any<sizeof(test), false, true>;

        test value{'X'};
        EXPECT_FALSE(value.moved_);
        EXPECT_EQ('X', value.payload_);

        test value2{std::move(value)};
        EXPECT_TRUE(value.moved_);
        EXPECT_EQ('\0', value.payload_);
        EXPECT_FALSE(value2.moved_);
        EXPECT_EQ(1, value2.value_);
        EXPECT_EQ('X', value2.payload_);
        // uut src{value}; //< expectedly won't compile (due to !copyable `test`)
        uut src{std::move(value2)};
        EXPECT_EQ('X', any_cast<test&>(src).payload_);
        // const uut  dst{src}; //< expectedly won't compile (due to !copyable `any`)
    }

    // Non-Copyable and non-movable `any`
    {
        using test = TestCopyableAndMovable;
        using uut  = any<sizeof(test), false>;

        uut src{test{}};
        EXPECT_EQ(1 + 10, any_cast<test>(src).value_);
        EXPECT_EQ(1, any_cast<test&>(src).value_);
        EXPECT_EQ(1 + 1, any_cast<test>(std::move(src)).value_);
        // const uut  dst{src}; //< expectedly won't compile (due to !copyable `any`)
        // uut dst{std::move(src)}; //< expectedly won't compile (due to !movable `any`)
    }
}

TEST(test_any, ctor_3_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut       src{42};
        const uut dst{std::move(src)};

        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(42, any_cast<int>(dst));
    }

    // Copyable and Movable `any`
    {
        using test = TestCopyableAndMovable;
        using uut  = any<sizeof(test)>;

        uut src{test{}};
        EXPECT_TRUE(src.has_value());

        const uut dst{std::move(src)};
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(2, any_cast<const TestCopyableAndMovable&>(dst).value_);
    }

    // Movable only `any`
    {
        using test = TestMovableOnly;
        using uut  = any<sizeof(test), false, true>;

        uut       src{test{'X'}};
        const uut dst{std::move(src)};

        EXPECT_EQ(nullptr, any_cast<test>(&src));
        EXPECT_EQ(2, any_cast<const test&>(dst).value_);
        EXPECT_EQ('X', any_cast<const test&>(dst).payload_);
        // EXPECT_EQ(2, any_cast<test>(dst).value_); //< expectedly won't compile (due to !copyable)
        // EXPECT_EQ(2, any_cast<test&>(dst).value_); //< expectedly won't compile (due to const)
    }

    // Copyable only `any`, movable only `unique_ptr`
    {
        using test = std::unique_ptr<TestCopyableAndMovable>;
        using uut  = any<sizeof(test), false, true>;

        uut src{std::make_unique<TestCopyableAndMovable>()};
        uut dst{std::move(src)};
        EXPECT_FALSE(src.has_value());

        auto ptr = any_cast<test>(std::move(dst));
        EXPECT_TRUE(ptr);
        EXPECT_EQ(0, ptr->value_);
    }
}

TEST(test_any, ctor_4_move_value)
{
    using test = TestCopyableAndMovable;
    using uut  = any<sizeof(test)>;

    test      value{'Y'};
    const uut dst{std::move(value)};
    EXPECT_TRUE(value.moved_);
    EXPECT_TRUE(dst.has_value());
    EXPECT_EQ(1, any_cast<const test&>(dst).value_);
    EXPECT_EQ('Y', any_cast<const test&>(dst).payload_);
}

TEST(test_any, ctor_5_in_place)
{
    struct TestType : rtti_helper<type_id_type<42>>
    {
        char ch_;
        int  number_;

        TestType(const char ch, const int number)
        {
            ch_     = ch;
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const uut src{in_place_type_t<TestType>{}, 'Y', 42};

    const auto test = any_cast<TestType>(src);
    EXPECT_EQ('Y', test.ch_);
    EXPECT_EQ(42, test.number_);
}

TEST(test_any, ctor_6_in_place_initializer_list)
{
    struct TestType : rtti_helper<type_id_type<42>>
    {
        std::size_t size_;
        int         number_;

        TestType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const uut src{in_place_type_t<TestType>{}, {'A', 'B', 'C'}, 42};

    auto& test = any_cast<const TestType&>(src);
    EXPECT_EQ(3, test.size_);
    EXPECT_EQ(42, test.number_);
}

TEST(test_any, assign_1_copy)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        const uut src{42};
        EXPECT_TRUE(src.has_value());

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = src;
        EXPECT_TRUE(src.has_value());
        EXPECT_TRUE(dst.has_value());
        EXPECT_EQ(42, any_cast<int>(dst));

        const uut src2{147};
        dst = src2;
        EXPECT_EQ(147, any_cast<int>(dst));

        const uut empty{};
        dst = empty;
        EXPECT_FALSE(dst.has_value());
    }

    // Copyable only `any`
    //
    side_effect_stats stats;
    {
        using test = TestCopyableOnly;
        using uut  = any<sizeof(test), true, false>;

        auto side_effects = stats.make_side_effect_fn();

        const test value1{'X', side_effects};
        EXPECT_STREQ("@", stats.ops.c_str());

        const uut src1{value1};
        EXPECT_STREQ("@C", stats.ops.c_str());

        uut dst{};
        dst = src1;
        EXPECT_STREQ("@CCC~", stats.ops.c_str());

        EXPECT_EQ(10, any_cast<const test&>(src1).value_);
        EXPECT_EQ('X', any_cast<const test&>(src1).payload_);
        EXPECT_EQ(30, any_cast<const test&>(dst).value_);
        EXPECT_EQ('X', any_cast<const test&>(dst).payload_);

        const test value2{'Z', side_effects};
        EXPECT_STREQ("@CCC~@", stats.ops.c_str());

        const uut src2{value2};
        EXPECT_STREQ("@CCC~@C", stats.ops.c_str());

        dst = src2;
        EXPECT_STREQ("@CCC~@CCC~C~C~~", stats.ops.c_str());

        auto dst_ptr = &dst;
        dst          = *dst_ptr;
        EXPECT_STREQ("@CCC~@CCC~C~C~~", stats.ops.c_str());

        EXPECT_EQ(10, any_cast<const test&>(src2).value_);
        EXPECT_EQ('Z', any_cast<const test&>(src2).payload_);
        EXPECT_EQ(30, any_cast<const test&>(dst).value_);
        EXPECT_EQ('Z', any_cast<const test&>(dst).payload_);
    }
    EXPECT_EQ(stats.constructs, stats.destructs);
    EXPECT_STREQ("@CCC~@CCC~C~C~~~~~~~", stats.ops.c_str());
}

TEST(test_any, assign_2_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut src{42};
        EXPECT_TRUE(src.has_value());

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = std::move(src);
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(42, any_cast<int>(dst));

        dst = uut{147};
        EXPECT_EQ(147, any_cast<int>(dst));

        auto dst_ptr = &dst;
        dst          = std::move(*dst_ptr);
        EXPECT_EQ(147, any_cast<int>(dst));

        dst = uut{};
        EXPECT_FALSE(dst.has_value());
    }

    // Movable only `any`
    //
    side_effect_stats stats;
    {
        using test = TestMovableOnly;
        using uut  = any<sizeof(test), false, true>;

        auto side_effects = stats.make_side_effect_fn();

        uut src1{test{'X', side_effects}};
        EXPECT_STREQ("@M_", stats.ops.c_str());

        uut dst{};
        dst = std::move(src1);
        EXPECT_STREQ("@M_M_M_", stats.ops.c_str());

        EXPECT_EQ(nullptr, any_cast<test>(&src1));
        EXPECT_EQ(3, any_cast<const test&>(dst).value_);
        EXPECT_EQ('X', any_cast<const test&>(dst).payload_);

        uut src2{test{'Z', side_effects}};
        EXPECT_STREQ("@M_M_M_@M_", stats.ops.c_str());

        dst = std::move(src2);
        EXPECT_STREQ("@M_M_M_@M_M_M_M_M_~", stats.ops.c_str());

        EXPECT_EQ(nullptr, any_cast<test>(&src2));
        EXPECT_EQ(3, any_cast<const test&>(dst).value_);
        EXPECT_EQ('Z', any_cast<const test&>(dst).payload_);
    }
    EXPECT_EQ(stats.constructs, stats.destructs);
    EXPECT_STREQ("@M_M_M_@M_M_M_M_M_~~", stats.ops.c_str());
}

TEST(test_any, assign_3_move_value)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = 147;
        EXPECT_EQ(147, any_cast<int>(dst));
    }
}
/*
TEST(test_any, make_any_cppref_example)
{
    using uut = any<std::max(sizeof(std::string), sizeof(std::complex<double>))>;

    auto a0 = make_any<std::string, uut>("Hello, cetl::any!\n");
    auto a1 = make_any<std::complex<double>, uut>(0.1, 2.3);

    EXPECT_STREQ("Hello, cetl::any!\n", any_cast<std::string>(a0).c_str());
    EXPECT_EQ(std::complex<double>(0.1, 2.3), any_cast<std::complex<double>>(a1));

    // TODO: Add more from the example when corresponding api will be available.
    // https://en.cppreference.com/w/cpp/utility/any/make_any
    using lambda     = std::function<const char*(void)>;
    using any_lambda = any<sizeof(lambda)>;

    const any_lambda a2 = [] { return "Lambda #1.\n"; };

    EXPECT_TRUE(a2.has_value());
    // TODO: Uncomment when RTTI will be available.
    // auto function2 = any_cast<lambda>(a2);

    auto a3 = make_any<lambda, any_lambda>([] { return "Lambda #2.\n"; });
    EXPECT_TRUE(a3.has_value());
    const auto function3 = any_cast<lambda>(a3);
    EXPECT_STREQ("Lambda #2.\n", function3());
}
*/
TEST(test_any, make_any_1)
{
    using uut = const any<sizeof(int)>;

    const auto test = make_any<int, uut>(42);
    EXPECT_EQ(42, any_cast<int>(test));
}

TEST(test_any, make_any_2_list)
{
    struct TestType : rtti_helper<type_id_type<13>>
    {
        std::size_t size_;
        int         number_;

        TestType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const auto  src  = make_any<TestType, uut>({'A', 'C'}, 42);
    const auto& test = any_cast<const TestType&>(src);
    EXPECT_EQ(2, test.size_);
    EXPECT_EQ(42, test.number_);
}

TEST(test_any, any_cast_cppref_example)
{
    using any = any<std::max(sizeof(int), sizeof(std::string))>;

    auto a1 = any{12};
    EXPECT_EQ(12, any_cast<int>(a1));

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(any_cast<std::string>(a1)), cetl::bad_any_cast);

#endif

    // Pointer example
    EXPECT_EQ(12, *any_cast<int>(&a1));
    EXPECT_EQ(nullptr, any_cast<std::string>(&a1));

    // Advanced example
    a1       = std::string("hello");
    auto& ra = any_cast<std::string&>(a1);  //< reference
    ra[1]    = 'o';
    EXPECT_STREQ("hollo", any_cast<const std::string&>(a1).c_str());  //< const reference

    auto s1 = any_cast<std::string&&>(std::move(a1));  //< rvalue reference
    // Note: `s1` is a move-constructed std::string, `a1` is empty
    static_assert(std::is_same<decltype(s1), std::string>::value, "");
    EXPECT_STREQ("hollo", s1.c_str());
}

TEST(test_any, any_cast_1_const)
{
    using any = any<std::max(sizeof(int), sizeof(std::string))>;

    const any src{42};

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(any_cast<std::string>(src)), cetl::bad_any_cast);

    const any empty{};
    EXPECT_THROW(sink(any_cast<std::string>(empty)), cetl::bad_any_cast);

#endif

    EXPECT_EQ(42, any_cast<int>(src));
    // EXPECT_EQ(42, any_cast<int&>(src)); //< won't compile expectedly
    EXPECT_EQ(42, any_cast<const int>(src));
    EXPECT_EQ(42, any_cast<const int&>(src));
}

TEST(test_any, any_cast_2_non_const)
{
    using any = any<std::max(sizeof(int), sizeof(std::string))>;

    any src{42};

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(any_cast<std::string>(src)), cetl::bad_any_cast);

    any empty{};
    EXPECT_THROW(sink(any_cast<std::string>(empty)), cetl::bad_any_cast);

#endif

    EXPECT_EQ(42, any_cast<int>(src));
    EXPECT_EQ(42, any_cast<int&>(src));
    EXPECT_EQ(42, any_cast<const int>(src));
    EXPECT_EQ(42, any_cast<const int&>(src));

    const auto test_str{"0123456789012345678901234567890123456789"s};

    src = test_str;
    EXPECT_STREQ(test_str.c_str(), any_cast<std::string>(src).c_str());

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(any_cast<int>(src)), cetl::bad_any_cast);

    src.reset();
    EXPECT_THROW(sink(any_cast<int>(src)), cetl::bad_any_cast);
    EXPECT_THROW(sink(any_cast<std::string>(src)), cetl::bad_any_cast);

#endif
}

TEST(test_any, any_cast_3_move_primitive_int)
{
    using any = any<sizeof(int)>;

    any src{147};
    EXPECT_EQ(147, any_cast<int>(std::move(src)));
    EXPECT_TRUE(src.has_value());  //< technically still "has" the value, but moved out.

    EXPECT_EQ(42, any_cast<int>(any{42}));
    // EXPECT_EQ(42, any_cast<int&>(uut{42})); //< won't compile expectedly
    EXPECT_EQ(42, any_cast<const int>(any{42}));
    EXPECT_EQ(42, any_cast<const int&>(any{42}));
}

TEST(test_any, any_cast_3_move_empty_bad_cast)
{
#if defined(__cpp_exceptions)

    using any = any<std::max(sizeof(int), sizeof(std::string))>;

    EXPECT_THROW(sink(any_cast<std::string>(any{})), cetl::bad_any_cast);

    const auto test_str{"0123456789012345678901234567890123456789"s};

    any src{test_str};

    // Try move out but with wrong type
    //
    EXPECT_THROW(sink(any_cast<int>(std::move(src))), cetl::bad_any_cast);
    //
    EXPECT_TRUE(src.has_value());  //< expectedly still has value b/c there was exception
    EXPECT_STREQ(test_str.c_str(), any_cast<std::string&>(src).c_str());

    // Retry to move out but now with correct type
    //
    EXPECT_STREQ(test_str.c_str(), any_cast<std::string>(std::move(src)).c_str());
    //
    EXPECT_TRUE(src.has_value());  //< technically still "has" the value, but moved out (hence empty).
    EXPECT_TRUE(any_cast<std::string&>(src).empty());

#endif
}

TEST(test_any, any_cast_4_const_ptr)
{
    using any = const any<sizeof(int)>;

    any src{147};

    auto int_ptr = any_cast<int>(&src);
    static_assert(std::is_same<const int*, decltype(int_ptr)>::value, "");

    EXPECT_TRUE(int_ptr);
    EXPECT_EQ(147, *int_ptr);

    EXPECT_FALSE((any_cast<char, any>(nullptr)));
}

TEST(test_any, any_cast_5_non_const_ptr_with_custom_alignment)
{
    constexpr std::size_t alignment = 4096;

    using any = any<sizeof(char), true, true, alignment>;

    any src{'Y'};

    auto char_ptr = any_cast<char>(&src);
    static_assert(std::is_same<char*, decltype(char_ptr)>::value, "");
    EXPECT_TRUE(char_ptr);
    EXPECT_EQ('Y', *char_ptr);
    EXPECT_EQ(0, reinterpret_cast<intptr_t>(char_ptr) & static_cast<std::intptr_t>(alignment - 1));

    EXPECT_FALSE((any_cast<char, any>(static_cast<any*>(nullptr))));
}

TEST(test_any, any_cast_polymorphic)
{
    side_effect_stats stats;
    {
        using any = any<sizeof(TestCopyableAndMovable)>;

        auto side_effects = stats.make_side_effect_fn();

        any test_any = TestCopyableAndMovable{'Y', side_effects};

        auto& test_base1 = any_cast<const TestBase&>(test_any);
        EXPECT_EQ('Y', test_base1.payload_);
        EXPECT_STREQ("TestCopyableAndMovable", test_base1.what());
        EXPECT_NE(nullptr, any_cast<TestCopyableAndMovable>(&test_any));
        EXPECT_EQ(nullptr, any_cast<TestCopyableOnly>(&test_any));
        EXPECT_EQ(nullptr, any_cast<TestMovableOnly>(&test_any));

        test_any = TestBase{'X', side_effects};

        auto& test_base2 = any_cast<const TestBase&>(test_any);
        EXPECT_EQ('X', test_base2.payload_);
        EXPECT_STREQ("TestBase", test_base2.what());
        EXPECT_EQ(nullptr, any_cast<TestCopyableAndMovable>(&test_any));
        EXPECT_EQ(nullptr, any_cast<TestCopyableOnly>(&test_any));
        EXPECT_EQ(nullptr, any_cast<TestMovableOnly>(&test_any));
    }
    EXPECT_EQ(stats.constructs, stats.destructs);
    EXPECT_STREQ("@M_@MM_M_M_~_~", stats.ops.c_str());
}

TEST(test_any, swap_copyable)
{
    using test = TestCopyableOnly;
    using any  = any<sizeof(test), true, false>;

    any empty{};
    any a{in_place_type_t<test>{}, 'A'};
    any b{in_place_type_t<test>{}, 'B'};

    // Self swap
    a.swap(a);
    EXPECT_EQ('A', any_cast<const test&>(a).payload_);
    // EXPECT_EQ(nullptr, any_cast<TestCopyableAndMovable>(&a)); //< won't compile expectedly b/c footprint is smaller

    a.swap(b);
    EXPECT_EQ('B', any_cast<test&>(a).payload_);
    EXPECT_EQ('A', any_cast<test&>(b).payload_);

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ('B', any_cast<test&>(empty).payload_);

    empty.swap(a);
    EXPECT_FALSE(empty.has_value());
    EXPECT_EQ('B', any_cast<test&>(a).payload_);

    any another_empty{};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
}

TEST(test_any, swap_movable)
{
    using test = TestMovableOnly;
    using uut  = any<sizeof(test), false, true>;

    uut empty{};
    uut a{in_place_type_t<test>{}, 'A'};
    uut b{in_place_type_t<test>{}, 'B'};

    // Self swap
    a.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(any_cast<test&>(a).moved_);
    EXPECT_EQ('A', any_cast<const test&>(a).payload_);

    a.swap(b);
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_FALSE(any_cast<test&>(a).moved_);
    EXPECT_FALSE(any_cast<test&>(b).moved_);
    EXPECT_EQ('B', any_cast<test&>(a).payload_);
    EXPECT_EQ('A', any_cast<test&>(b).payload_);

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(empty.has_value());
    EXPECT_FALSE(any_cast<test&>(empty).moved_);
    EXPECT_EQ('B', any_cast<test&>(empty).payload_);

    empty.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(any_cast<test&>(a).moved_);
    EXPECT_EQ('B', any_cast<test&>(a).payload_);

    uut another_empty{};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
}

TEST(test_any, emplace_1)
{
    // Primitive `char`
    {
        using uut = any<sizeof(char)>;

        uut src;
        src.emplace<char>('Y');
        EXPECT_EQ('Y', any_cast<char>(src));
    }

    // `TestType` with two params ctor.
    {
        struct TestType : rtti_helper<type_id_type<13>>
        {
            char ch_;
            int  number_;

            TestType(char ch, int number)
            {
                ch_     = ch;
                number_ = number;
            }
        };
        using uut = any<sizeof(TestType)>;

        uut t;
        t.emplace<TestType>('Y', 147);
        EXPECT_EQ('Y', any_cast<TestType>(t).ch_);
        EXPECT_EQ(147, any_cast<TestType>(t).number_);
    }
}

TEST(test_any, emplace_2_initializer_list)
{
    struct TestType : rtti_helper<type_id_type<13>>
    {
        std::size_t size_;
        int         number_;

        TestType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    uut src;
    src.emplace<TestType>({'A', 'B', 'C'}, 42);

    const auto test = any_cast<TestType>(src);
    EXPECT_EQ(3, test.size_);
    EXPECT_EQ(42, test.number_);
}

TEST(test_any, type)
{
#if __cpp_rtti

    using uut = any<std::max(sizeof(TestMovableOnly), sizeof(TestCopyableAndMovable)), false, true>;

    uut dst{};
    EXPECT_EQ(typeid(void), dst.type());

    dst = TestMovableOnly{};
    EXPECT_EQ(typeid(TestMovableOnly), dst.type());

    dst = TestCopyableAndMovable{};
    EXPECT_EQ(typeid(TestCopyableAndMovable), dst.type());

    dst.reset();
    EXPECT_EQ(typeid(void), dst.type());

#endif
}
}  // namespace

namespace cetl
{
template <>
constexpr type_id type_id_value<bool> = {1};

template <>
constexpr type_id type_id_value<int> = {2};

template <>
constexpr type_id type_id_value<float> = {3};

template <>
constexpr type_id type_id_value<double> = {4};

template <>
constexpr type_id type_id_value<char> = {5};

template <>
constexpr type_id type_id_value<std::string> = {6};

template <>
constexpr type_id type_id_value<std::unique_ptr<TestCopyableAndMovable>> =
    {0xB3, 0xB8, 0x4E, 0xC1, 0x1F, 0xE4, 0x49, 0x35, 0x9E, 0xC9, 0x1A, 0x77, 0x7B, 0x82, 0x53, 0x25};

template <>
constexpr type_id type_id_value<std::complex<double>> = {8};

template <>
constexpr type_id type_id_value<std::function<const char*()>> = {9};

}  // namespace cetl

// NOLINTEND(*-use-after-move)
