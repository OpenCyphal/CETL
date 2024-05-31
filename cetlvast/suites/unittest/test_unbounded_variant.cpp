/// @file
/// Unit tests for cetl/unbounded_variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/unbounded_variant.hpp>
#include <cetlvast/memory_resource_mock.hpp>
#include <cetlvast/tracking_memory_resource.hpp>

#include <complex>
#include <functional>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>
#include <vector>

// NOLINTBEGIN(*-use-after-move)

namespace
{

using cetl::unbounded_variant;
using cetl::get;
using cetl::get_if;
using cetl::make_unbounded_variant;
using cetl::in_place_type_t;
using cetl::type_id;
using cetl::type_id_type;
using cetl::rtti_helper;

using testing::_;
using testing::Return;
using testing::IsNull;
using testing::IsEmpty;
using testing::NotNull;
using testing::InSequence;
using testing::StrictMock;

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

    void reset()
    {
        ops.clear();
        assignments = 0;
        constructs  = 0;
        destructs   = 0;
    }

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

struct MyBase : rtti_helper<type_id_type<0x0>>
{
    char payload_;
    int  value_ = 0;
    bool moved_ = false;

    MyBase(const char payload, side_effect_fn side_effect)
        : payload_(payload)
        , side_effect_(std::move(side_effect))
    {
        side_effect_(side_effect_op::Construct);
    }
    MyBase(const MyBase& other)
    {
        copy_from(other, side_effect_op::CopyConstruct);
    }
    MyBase(MyBase&& other) noexcept
    {
        move_from(other, side_effect_op::MoveConstruct);
    }

    ~MyBase() override
    {
        side_effect_(moved_ ? side_effect_op::DestructMoved : side_effect_op::Destruct);
    }

    MyBase& operator=(const MyBase& other)
    {
        copy_from(other, side_effect_op::CopyAssign);
        return *this;
    }

    MyBase& operator=(MyBase&& other) noexcept
    {
        move_from(other, side_effect_op::MoveAssign);
        return *this;
    }

    CETL_NODISCARD virtual const char* what() const noexcept
    {
        return "MyBase";
    }

private:
    side_effect_fn side_effect_;

    void copy_from(const MyBase& other, const side_effect_op op)
    {
        payload_     = other.payload_;
        side_effect_ = other.side_effect_;
        value_       = other.value_ + 10;

        side_effect_(op);
    }

    void move_from(MyBase& other, const side_effect_op op)
    {
        payload_     = other.payload_;
        side_effect_ = other.side_effect_;
        value_       = other.value_ + 1;

        other.moved_   = true;
        other.payload_ = '\0';

        side_effect_(op);
    }

};  // MyBase

struct MyCopyableOnly final : MyBase
{
    explicit MyCopyableOnly(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : MyBase(payload, std::move(side_effect))
    {
    }
    MyCopyableOnly(const MyCopyableOnly& other)     = default;
    MyCopyableOnly(MyCopyableOnly&& other) noexcept = delete;

    MyCopyableOnly& operator=(const MyCopyableOnly& other)     = default;
    MyCopyableOnly& operator=(MyCopyableOnly&& other) noexcept = delete;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "MyCopyableOnly";
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
    using base = MyBase;
};

struct MyMovableOnly final : MyBase
{
    explicit MyMovableOnly(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : MyBase(payload, std::move(side_effect))
    {
    }
    MyMovableOnly(const MyMovableOnly& other)     = delete;
    MyMovableOnly(MyMovableOnly&& other) noexcept = default;

    MyMovableOnly& operator=(const MyMovableOnly& other)     = delete;
    MyMovableOnly& operator=(MyMovableOnly&& other) noexcept = default;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "MyMovableOnly";
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
    using base = MyBase;
};

struct MyCopyableAndMovable final : MyBase
{
    // Just to make this class a bit bigger than base.
    char place_holder_;

    explicit MyCopyableAndMovable(
        const char     payload     = '?',
        side_effect_fn side_effect = [](auto) {})
        : MyBase(payload, std::move(side_effect))
        , place_holder_{payload}
    {
    }

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "MyCopyableAndMovable";
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
    using base = MyBase;
};

struct Empty
{};

class TestPmrUnboundedVariant : public testing::Test
{
protected:
    using pmr = cetl::pmr::memory_resource;

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);
    }

    pmr* get_mr() noexcept
    {
        return &mr_;
    }

    pmr* get_default_mr() noexcept
    {
        return cetl::pmr::get_default_resource();
    }

    cetlvast::TrackingMemoryResource mr_;
};

/// TESTS -----------------------------------------------------------------------------------------------------------

TEST_F(TestPmrUnboundedVariant, bad_unbounded_variant_access_ctor)
{
#if defined(__cpp_exceptions)

    // Test the default constructor.
    cetl::bad_unbounded_variant_access test_exception1;

    // Test the copy constructor.
    cetl::bad_unbounded_variant_access test_exception2{test_exception1};

    // Test the move constructor.
    cetl::bad_unbounded_variant_access test_exception3{std::move(test_exception2)};
    EXPECT_THAT(test_exception3.what(), "bad unbounded variant access");

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

TEST_F(TestPmrUnboundedVariant, bad_unbounded_variant_access_assignment)
{
#if defined(__cpp_exceptions)

    // Test the copy assignment operator.
    cetl::bad_unbounded_variant_access test_exception1;
    cetl::bad_unbounded_variant_access test_exception2;
    test_exception2 = test_exception1;

    // Test the move assignment operator.
    cetl::bad_unbounded_variant_access test_exception3;
    test_exception3 = std::move(test_exception2);
    EXPECT_THAT(test_exception3.what(), "bad unbounded variant access");

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

TEST_F(TestPmrUnboundedVariant, cppref_example)
{
    using ub_var = unbounded_variant<std::max(sizeof(int), sizeof(double))>;

    ub_var a = 1;
    EXPECT_THAT(get<int>(a), 1);

    a = 3.14;
    EXPECT_THAT(get<double>(a), 3.14);

    a = true;
    EXPECT_TRUE(get<bool>(a));

    // bad cast
    a = 1;
#if defined(__cpp_exceptions)
    EXPECT_THROW(sink(get<float>(a)), cetl::bad_unbounded_variant_access);
#else
    EXPECT_THAT(get_if<float>(&a), IsNull());
#endif

    a = 2;
    EXPECT_TRUE(a.has_value());

    // reset
    a.reset();
    ASSERT_FALSE(a.has_value());

    // pointer to contained data
    a = 3;
    EXPECT_THAT(*get_if<int>(&a), 3);
}

TEST_F(TestPmrUnboundedVariant, ctor_1_default)
{
    EXPECT_FALSE((unbounded_variant<1>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, false>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, false, true>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, true, false>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, true, true, 1>{}.has_value()));

    EXPECT_FALSE((unbounded_variant<13>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, false>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, false, true>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, true, false>{}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, true, true, 1>{}.has_value()));
}

TEST_F(TestPmrUnboundedVariant, ctor_1_default_pmr)
{
    EXPECT_FALSE((unbounded_variant<0, false, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<0, false, true, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<0, true, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<0, true, true, 8, pmr>{get_mr()}.has_value()));

    EXPECT_FALSE((unbounded_variant<1, false, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, false, true, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, true, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<1, true, true, 8, pmr>{get_mr()}.has_value()));

    EXPECT_FALSE((unbounded_variant<13, false, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, false, true, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, true, false, 8, pmr>{get_mr()}.has_value()));
    EXPECT_FALSE((unbounded_variant<13, true, true, 8, pmr>{get_mr()}.has_value()));
}

TEST_F(TestPmrUnboundedVariant, ctor_2_copy)
{
    // Primitive `int`
    {
        using ub_var = unbounded_variant<sizeof(int)>;

        const ub_var src{42};
        ub_var       dst{src};

        EXPECT_THAT(get<int>(src), 42);
        EXPECT_THAT(get<int>(dst), 42);

        const ub_var empty{};
        ub_var       dst2{empty};
        EXPECT_THAT(dst2.has_value(), false);
        dst2 = {};
        EXPECT_THAT(dst2.has_value(), false);
    }

    // Copyable and Movable `unbounded_variant`
    {
        using test   = MyCopyableAndMovable;
        using ub_var = unbounded_variant<sizeof(test)>;

        const ub_var src{test{}};
        ub_var       dst{src};

        EXPECT_THAT(get<test>(src).value_, 1 + 10);
        EXPECT_THAT(get<const test&>(src).value_, 1);

        EXPECT_THAT(get<test>(dst).value_, 1 + 10 + 10);
        EXPECT_THAT(get<test&>(dst).value_, 1 + 10);
        EXPECT_THAT(get<const test&>(dst).value_, 1 + 10);

        EXPECT_FALSE(get<const test&>(dst).moved_);
        EXPECT_THAT(get<test>(std::move(dst)).value_, 1 + 10 + 1);
        EXPECT_TRUE(get<const test&>(dst).moved_);
    }

    // Copyable only `unbounded_variant`
    {
        using test   = MyCopyableOnly;
        using ub_var = unbounded_variant<sizeof(test), true, false>;

        const test   value{};
        ub_var       src{value};
        const ub_var dst{src};

        EXPECT_THAT(get<test&>(src).value_, 10);
        EXPECT_THAT(get<const test&>(src).value_, 10);

        EXPECT_THAT(get<const test&>(dst).value_, 10 + 10);
    }

    // Movable only `unbounded_variant`
    {
        using test   = MyMovableOnly;
        using ub_var = unbounded_variant<sizeof(test), false, true>;

        test value{'X'};
        EXPECT_FALSE(value.moved_);
        EXPECT_THAT(value.payload_, 'X');

        test value2{std::move(value)};
        EXPECT_TRUE(value.moved_);
        EXPECT_THAT(value.payload_, '\0');
        EXPECT_FALSE(value2.moved_);
        EXPECT_THAT(value2.value_, 1);
        EXPECT_THAT(value2.payload_, 'X');
        // ub_var src{value}; //< expectedly won't compile (due to !copyable `test`)
        ub_var src{std::move(value2)};
        EXPECT_THAT(get<test&>(src).payload_, 'X');
        // const ub_var dst{src}; //< expectedly won't compile (due to !copyable `unbounded_variant`)
    }

    // Non-Copyable and non-movable `unbounded_variant`
    {
        using test   = MyCopyableAndMovable;
        using ub_var = unbounded_variant<sizeof(test), false>;

        ub_var src{test{}};
        EXPECT_THAT(get<test>(src).value_, 1 + 10);
        EXPECT_THAT(get<test&>(src).value_, 1);
        EXPECT_THAT(get<test>(std::move(src)).value_, 1 + 1);
        // const ub_var dst{src}; //< expectedly won't compile (due to !copyable `unbounded_variant`)
        // ub_var dst{std::move(src)}; //< expectedly won't compile (due to !movable `unbounded_variant`)
    }
}

TEST_F(TestPmrUnboundedVariant, ctor_3_move)
{
    // Primitive `int`
    {
        using ub_var = unbounded_variant<sizeof(int)>;

        ub_var       src{42};
        const ub_var dst{std::move(src)};

        EXPECT_FALSE(src.has_value());
        EXPECT_THAT(get<int>(dst), 42);

        ub_var empty{};
        ub_var dst2{std::move(empty)};
        EXPECT_THAT(dst2.has_value(), false);
    }

    // Copyable and Movable `unbounded_variant`
    {
        using test   = MyCopyableAndMovable;
        using ub_var = unbounded_variant<sizeof(test)>;

        ub_var src{test{}};
        EXPECT_TRUE(src.has_value());

        const ub_var dst{std::move(src)};
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_THAT(get<const MyCopyableAndMovable&>(dst).value_, 2);
    }

    // Movable only `unbounded_variant`
    {
        using test   = MyMovableOnly;
        using ub_var = unbounded_variant<sizeof(test), false, true>;

        ub_var       src{test{'X'}};
        const ub_var dst{std::move(src)};

        EXPECT_THAT(get_if<test>(&src), IsNull());
        EXPECT_THAT(get<const test&>(dst).value_, 2);
        EXPECT_THAT(get<const test&>(dst).payload_, 'X');
        // EXPECT_THAT(get<test>(dst).value_, 2); //< expectedly won't compile (due to !copyable)
        // EXPECT_THAT(get<test&>(dst).value_, 2); //< expectedly won't compile (due to const)
    }

    // Copyable only `unbounded_variant`, movable only `unique_ptr`
    {
        using test   = std::unique_ptr<MyCopyableAndMovable>;
        using ub_var = unbounded_variant<sizeof(test), false, true>;

        ub_var src{std::make_unique<MyCopyableAndMovable>()};
        ub_var dst{std::move(src)};
        EXPECT_FALSE(src.has_value());

        auto ptr = get<test>(std::move(dst));
        EXPECT_TRUE(ptr);
        EXPECT_THAT(ptr->value_, 0);
    }
}

TEST_F(TestPmrUnboundedVariant, ctor_4_move_value)
{
    using test   = MyCopyableAndMovable;
    using ub_var = unbounded_variant<sizeof(test)>;

    test         value{'Y'};
    const ub_var dst{std::move(value)};
    EXPECT_TRUE(value.moved_);
    EXPECT_TRUE(dst.has_value());
    EXPECT_THAT(get<const test&>(dst).value_, 1);
    EXPECT_THAT(get<const test&>(dst).payload_, 'Y');
}

TEST_F(TestPmrUnboundedVariant, ctor_5_in_place)
{
    struct MyType : rtti_helper<type_id_type<42>>
    {
        char ch_;
        int  number_;

        MyType(const char ch, const int number)
        {
            ch_     = ch;
            number_ = number;
        }
    };
    using ub_var = unbounded_variant<sizeof(MyType)>;

    const ub_var src{in_place_type_t<MyType>{}, 'Y', 42};

    const auto test = get<MyType>(src);
    EXPECT_THAT(test.ch_, 'Y');
    EXPECT_THAT(test.number_, 42);
}

TEST_F(TestPmrUnboundedVariant, ctor_6_in_place_initializer_list)
{
    struct MyType : rtti_helper<type_id_type<42>>
    {
        std::size_t size_;
        int         number_;

        MyType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using ub_var = unbounded_variant<sizeof(MyType)>;

    const ub_var src{in_place_type_t<MyType>{}, {'A', 'B', 'C'}, 42};

    auto& test = get<const MyType&>(src);
    EXPECT_THAT(test.size_, 3);
    EXPECT_THAT(test.number_, 42);
}

TEST_F(TestPmrUnboundedVariant, assign_1_copy)
{
    // Primitive `int`
    {
        using ub_var = unbounded_variant<sizeof(int)>;

        const ub_var src{42};
        EXPECT_TRUE(src.has_value());

        ub_var dst{};
        EXPECT_FALSE(dst.has_value());

        dst = src;
        EXPECT_TRUE(src.has_value());
        EXPECT_TRUE(dst.has_value());
        EXPECT_THAT(get<int>(dst), 42);

        const ub_var src2{147};
        dst = src2;
        EXPECT_THAT(get<int>(dst), 147);

        const ub_var empty{};
        dst = empty;
        EXPECT_FALSE(dst.has_value());
    }

    // Copyable only `unbounded_variant`
    //
    side_effect_stats stats;
    {
        using test   = MyCopyableOnly;
        using ub_var = unbounded_variant<sizeof(test), true, false>;

        auto side_effects = stats.make_side_effect_fn();

        const test value1{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        const ub_var src1{value1};
        EXPECT_THAT(stats.ops, "@C");

        ub_var dst{};
        dst = src1;
        EXPECT_THAT(stats.ops, "@CC");

        EXPECT_THAT(get<const test&>(src1).value_, 10);
        EXPECT_THAT(get<const test&>(src1).payload_, 'X');
        EXPECT_THAT(get<const test&>(dst).value_, 20);
        EXPECT_THAT(get<const test&>(dst).payload_, 'X');

        const test value2{'Z', side_effects};
        EXPECT_THAT(stats.ops, "@CC@");

        const ub_var src2{value2};
        EXPECT_THAT(stats.ops, "@CC@C");

        dst = src2;
        EXPECT_THAT(stats.ops, "@CC@C~C");

        auto dst_ptr = &dst;
        dst          = *dst_ptr;
        EXPECT_THAT(stats.ops, "@CC@C~C");

        EXPECT_THAT(get<const test&>(src2).value_, 10);
        EXPECT_THAT(get<const test&>(src2).payload_, 'Z');
        EXPECT_THAT(get<const test&>(dst).value_, 20);
        EXPECT_THAT(get<const test&>(dst).payload_, 'Z');
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@CC@C~C~~~~~");
}

TEST_F(TestPmrUnboundedVariant, assign_2_move)
{
    // Primitive `int`
    {
        using ub_var = unbounded_variant<sizeof(int)>;

        ub_var src{42};
        EXPECT_TRUE(src.has_value());

        ub_var dst{};
        EXPECT_FALSE(dst.has_value());

        dst = std::move(src);
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_THAT(get<int>(dst), 42);

        dst = ub_var{147};
        EXPECT_THAT(get<int>(dst), 147);

        auto dst_ptr = &dst;
        dst          = std::move(*dst_ptr);
        EXPECT_THAT(get<int>(dst), 147);

        dst = ub_var{};
        EXPECT_FALSE(dst.has_value());
    }

    // Movable only `unbounded_variant`
    //
    side_effect_stats stats;
    {
        using test   = MyMovableOnly;
        using ub_var = unbounded_variant<sizeof(test), false, true>;

        auto side_effects = stats.make_side_effect_fn();

        ub_var src1{test{'X', side_effects}};
        EXPECT_THAT(stats.ops, "@M_");

        ub_var dst{};
        dst = std::move(src1);
        EXPECT_THAT(stats.ops, "@M_M_");

        EXPECT_THAT(get_if<test>(&src1), IsNull());
        EXPECT_THAT(get<const test&>(dst).value_, 2);
        EXPECT_THAT(get<const test&>(dst).payload_, 'X');

        ub_var src2{test{'Z', side_effects}};
        EXPECT_THAT(stats.ops, "@M_M_@M_");

        dst = std::move(src2);
        EXPECT_THAT(stats.ops, "@M_M_@M_~M_");

        EXPECT_THAT(get_if<test>(&src2), IsNull());
        EXPECT_THAT(get<const test&>(dst).value_, 2);
        EXPECT_THAT(get<const test&>(dst).payload_, 'Z');
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@M_M_@M_~M_~");
}

TEST_F(TestPmrUnboundedVariant, assign_3_move_value)
{
    // Primitive `int`
    {
        using ub_var = unbounded_variant<sizeof(int)>;

        ub_var dst{};
        EXPECT_FALSE(dst.has_value());

        dst = 147;
        EXPECT_THAT(get<int>(dst), 147);
    }
}

TEST_F(TestPmrUnboundedVariant, make_unbounded_variant_cppref_example)
{
    using ub_var = unbounded_variant<std::max(sizeof(std::string), sizeof(std::complex<double>))>;

    auto a0 = make_unbounded_variant<std::string, ub_var>("Hello, cetl::unbounded_variant!\n");
    auto a1 = make_unbounded_variant<std::complex<double>, ub_var>(0.1, 2.3);

    EXPECT_THAT(get<std::string>(a0), "Hello, cetl::unbounded_variant!\n");
    EXPECT_THAT(get<std::complex<double>>(a1), std::complex<double>(0.1, 2.3));

    using lambda        = std::function<const char*()>;
    using ub_var_lambda = cetl::unbounded_variant<sizeof(lambda)>;

    auto a3 = make_unbounded_variant<lambda, ub_var_lambda>([] { return "Lambda #3.\n"; });
    EXPECT_TRUE(a3.has_value());
    EXPECT_THAT(get<lambda>(a3)(), "Lambda #3.\n");
}

TEST_F(TestPmrUnboundedVariant, make_unbounded_variant_1)
{
    using ub_var = unbounded_variant<sizeof(int), false, true, 16>;

    auto src = make_unbounded_variant<int, ub_var>(42);
    EXPECT_THAT(get<int>(src), 42);
    static_assert(std::is_same<decltype(src), ub_var>::value, "");
}

TEST_F(TestPmrUnboundedVariant, make_unbounded_variant_1_like)
{
    auto src = make_unbounded_variant<uint16_t>(static_cast<uint16_t>(42));
    EXPECT_THAT(get<uint16_t>(src), 42);
    static_assert(std::is_same<decltype(src), cetl::unbounded_variant_like<uint16_t>>::value, "");
    static_assert(std::is_same<decltype(src),
                               cetl::unbounded_variant<sizeof(uint16_t), true, true, alignof(uint16_t)>>::value,
                  "");
}

TEST_F(TestPmrUnboundedVariant, make_unbounded_variant_2_list)
{
    struct MyType : rtti_helper<type_id_type<13>>
    {
        std::size_t size_;
        int         number_;

        MyType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using ub_var = unbounded_variant<sizeof(MyType)>;

    const auto  src  = make_unbounded_variant<MyType, ub_var>({'A', 'C'}, 42);
    const auto& test = get<const MyType&>(src);
    EXPECT_THAT(test.size_, 2);
    EXPECT_THAT(test.number_, 42);

    // `cetl::unbounded_variant_like` version
    //
    const auto dst = make_unbounded_variant<MyType>({'B', 'D', 'E'}, 147);
    static_assert(std::is_same<decltype(dst), const cetl::unbounded_variant_like<MyType>>::value, "");
    EXPECT_THAT(get_if<MyType>(&dst)->size_, 3);
    EXPECT_THAT(get<const MyType&>(dst).number_, 147);
}

TEST_F(TestPmrUnboundedVariant, get_cppref_example)
{
    using ub_var = unbounded_variant<std::max(sizeof(int), sizeof(std::string))>;

    auto a1 = ub_var{12};
    EXPECT_THAT(get<int>(a1), 12);

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(get<std::string>(a1)), cetl::bad_unbounded_variant_access);

#endif

    // Pointer example
    EXPECT_THAT(*get_if<int>(&a1), 12);
    EXPECT_THAT(get_if<std::string>(&a1), IsNull());

    // Advanced example
    a1       = std::string("hello");
    auto& ra = get<std::string&>(a1);  //< reference
    ra[1]    = 'o';
    EXPECT_THAT(get<const std::string&>(a1), "hollo");  //< const reference

    auto s1 = get<std::string&&>(std::move(a1));  //< rvalue reference
    // Note: `s1` is a move-constructed std::string, `a1` is empty
    static_assert(std::is_same<decltype(s1), std::string>::value, "");
    EXPECT_THAT(s1, "hollo");
}

TEST_F(TestPmrUnboundedVariant, get_1_const)
{
    using ub_var = unbounded_variant<std::max(sizeof(int), sizeof(std::string))>;

    const ub_var src{42};

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(get<std::string>(src)), cetl::bad_unbounded_variant_access);

    const ub_var empty{};
    EXPECT_THROW(sink(get<std::string>(empty)), cetl::bad_unbounded_variant_access);

#endif

    EXPECT_THAT(get<int>(src), 42);
    // EXPECT_THAT(42, get<int&>(src)); //< won't compile expectedly
    EXPECT_THAT(get<const int>(src), 42);
    EXPECT_THAT(get<const int&>(src), 42);
}

TEST_F(TestPmrUnboundedVariant, get_2_non_const)
{
    using ub_var = unbounded_variant<std::max(sizeof(int), sizeof(std::string))>;

    ub_var src{42};

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(get<std::string>(src)), cetl::bad_unbounded_variant_access);

    ub_var empty{};
    EXPECT_THROW(sink(get<std::string>(empty)), cetl::bad_unbounded_variant_access);

#endif

    EXPECT_THAT(get<int>(src), 42);
    EXPECT_THAT(get<int&>(src), 42);
    EXPECT_THAT(get<const int>(src), 42);
    EXPECT_THAT(get<const int&>(src), 42);

    const auto test_str = "0123456789012345678901234567890123456789"s;

    src = test_str;
    EXPECT_THAT(get<std::string>(src), test_str);

#if defined(__cpp_exceptions)

    EXPECT_THROW(sink(get<int>(src)), cetl::bad_unbounded_variant_access);

    src.reset();
    EXPECT_THROW(sink(get<int>(src)), cetl::bad_unbounded_variant_access);
    EXPECT_THROW(sink(get<std::string>(src)), cetl::bad_unbounded_variant_access);

#endif
}

TEST_F(TestPmrUnboundedVariant, get_3_move_primitive_int)
{
    using ub_var = unbounded_variant<sizeof(int)>;

    ub_var src{147};
    EXPECT_THAT(get<int>(std::move(src)), 147);
    EXPECT_TRUE(src.has_value());  //< technically still "has" the value, but moved out.

    EXPECT_THAT(get<int>(ub_var{42}), 42);
    // EXPECT_THAT(get<int&>(ub_var{42}), 42); //< won't compile expectedly
    EXPECT_THAT(get<const int>(ub_var{42}), 42);
    EXPECT_THAT(get<const int&>(ub_var{42}), 42);
}

TEST_F(TestPmrUnboundedVariant, get_3_move_empty_bad_cast)
{
#if defined(__cpp_exceptions)

    using ub_var = unbounded_variant<std::max(sizeof(int), sizeof(std::string))>;

    EXPECT_THROW(sink(get<std::string>(ub_var{})), cetl::bad_unbounded_variant_access);

    const auto test_str = "0123456789012345678901234567890123456789"s;

    ub_var src{test_str};

    // Try to move out but with wrong type
    //
    EXPECT_THROW(sink(get<int>(std::move(src))), cetl::bad_unbounded_variant_access);
    //
    EXPECT_TRUE(src.has_value());  //< expectedly still has value b/c there was exception
    EXPECT_THAT(get<std::string&>(src), test_str);

    // Retry to move out but now with correct type
    //
    EXPECT_THAT(get<std::string>(std::move(src)), test_str);
    //
    EXPECT_TRUE(src.has_value());  //< technically still "has" the value, but moved out (hence empty).
    EXPECT_TRUE(get<std::string&>(src).empty());

#endif
}

TEST_F(TestPmrUnboundedVariant, get_if_4_const_ptr)
{
    using ub_var = unbounded_variant<sizeof(int)>;

    const ub_var src{147};

    auto int_ptr = get_if<int>(&src);
    static_assert(std::is_same<const int*, decltype(int_ptr)>::value, "");

    EXPECT_TRUE(int_ptr);
    EXPECT_THAT(*int_ptr, 147);

    auto const_int_ptr = get_if<const int>(&src);
    static_assert(std::is_same<const int*, decltype(const_int_ptr)>::value, "");
    EXPECT_THAT(int_ptr, const_int_ptr);

    EXPECT_THAT(get_if<int>(static_cast<const ub_var*>(nullptr)), IsNull());
}

TEST_F(TestPmrUnboundedVariant, get_if_5_non_const_ptr_with_custom_alignment)
{
    constexpr std::size_t alignment = 4096;

    using ub_var = unbounded_variant<sizeof(char), true, true, alignment>;

    ub_var src{'Y'};

    auto char_ptr = get_if<char>(&src);
    static_assert(std::is_same<char*, decltype(char_ptr)>::value, "");
    EXPECT_TRUE(char_ptr);
    EXPECT_THAT(*char_ptr, 'Y');
    EXPECT_THAT(reinterpret_cast<intptr_t>(char_ptr) & static_cast<std::intptr_t>(alignment - 1), 0);

    auto const_char_ptr = get_if<const char>(&src);
    static_assert(std::is_same<const char*, decltype(const_char_ptr)>::value, "");
    EXPECT_THAT(char_ptr, const_char_ptr);

    EXPECT_THAT(get_if<char>(static_cast<ub_var*>(nullptr)), IsNull());
}

TEST_F(TestPmrUnboundedVariant, get_if_polymorphic)
{
    side_effect_stats stats;
    {
        using ub_var = unbounded_variant<sizeof(MyCopyableAndMovable)>;

        auto side_effects = stats.make_side_effect_fn();

        ub_var test_ubv = MyCopyableAndMovable{'Y', side_effects};

        auto& test_base1 = get<const MyBase&>(test_ubv);
        EXPECT_THAT(test_base1.payload_, 'Y');
        EXPECT_THAT(test_base1.what(), "MyCopyableAndMovable");
        EXPECT_THAT(get_if<MyCopyableAndMovable>(&test_ubv), NotNull());
        EXPECT_THAT(get_if<MyCopyableOnly>(&test_ubv), IsNull());
        EXPECT_THAT(get_if<MyMovableOnly>(&test_ubv), IsNull());

        test_ubv = MyBase{'X', side_effects};

        auto& test_base2 = get<const MyBase&>(test_ubv);
        EXPECT_THAT(test_base2.payload_, 'X');
        EXPECT_THAT(test_base2.what(), "MyBase");
        EXPECT_THAT(get_if<MyCopyableAndMovable>(&test_ubv), IsNull());
        EXPECT_THAT(get_if<MyCopyableOnly>(&test_ubv), IsNull());
        EXPECT_THAT(get_if<MyMovableOnly>(&test_ubv), IsNull());
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@M_@~M_~");
}

TEST_F(TestPmrUnboundedVariant, swap_copyable)
{
    using test   = MyCopyableOnly;
    using ub_var = unbounded_variant<sizeof(test), true, false>;

    ub_var empty{};
    ub_var a{in_place_type_t<test>{}, 'A'};
    ub_var b{in_place_type_t<test>{}, 'B'};

    // Self swap
    a.swap(a);
    EXPECT_THAT(get<const test&>(a).payload_, 'A');
    // EXPECT_THAT(get_if<MyCopyableAndMovable>(&a), IsNull()); //< won't compile expectedly b/c footprint is smaller

    a.swap(b);
    EXPECT_THAT(get<test&>(a).payload_, 'B');
    EXPECT_THAT(get<test&>(b).payload_, 'A');

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_THAT(get<test&>(empty).payload_, 'B');

    empty.swap(a);
    EXPECT_FALSE(empty.has_value());
    EXPECT_THAT(get<test&>(a).payload_, 'B');

    ub_var another_empty{};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
}

TEST_F(TestPmrUnboundedVariant, swap_movable)
{
    using test   = MyMovableOnly;
    using ub_var = unbounded_variant<sizeof(test), false, true>;

    ub_var empty{};
    ub_var a{in_place_type_t<test>{}, 'A'};
    ub_var b{in_place_type_t<test>{}, 'B'};

    // Self swap
    a.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_THAT(get<const test&>(a).payload_, 'A');

    a.swap(b);
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_FALSE(get<test&>(b).moved_);
    EXPECT_THAT(get<test&>(a).payload_, 'B');
    EXPECT_THAT(get<test&>(b).payload_, 'A');

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(empty.has_value());
    EXPECT_FALSE(get<test&>(empty).moved_);
    EXPECT_THAT(get<test&>(empty).payload_, 'B');

    empty.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_THAT(get<test&>(a).payload_, 'B');

    ub_var another_empty{};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
}

TEST_F(TestPmrUnboundedVariant, emplace_1)
{
    // Primitive `char`
    {
        using ub_var = unbounded_variant<sizeof(char)>;

        ub_var src;
        auto   y_ptr = src.emplace<char>('Y');
        EXPECT_THAT(get_if<char>(&src), y_ptr);
        EXPECT_THAT(get<char>(src), 'Y');
    }

    // `MyType` with two params ctor.
    {
        struct MyType : rtti_helper<type_id_type<13>>
        {
            char ch_;
            int  number_;

            MyType(char ch, int number)
            {
                ch_     = ch;
                number_ = number;
            }
        };
        using ub_var = unbounded_variant<sizeof(MyType)>;

        ub_var t;
        auto   my_ptr = t.emplace<MyType>('Y', 147);
        EXPECT_THAT(get_if<MyType>(&t), my_ptr);
        EXPECT_THAT(get<MyType>(t).ch_, 'Y');
        EXPECT_THAT(get<MyType>(t).number_, 147);
    }
}

TEST_F(TestPmrUnboundedVariant, emplace_1_ctor_exception)
{
    side_effect_stats stats;
    {
        using ub_var = unbounded_variant<sizeof(MyCopyableAndMovable)>;

        auto stats_side_effects    = stats.make_side_effect_fn();
        auto throwing_side_effects = [=](side_effect_op op) {
            stats_side_effects(op);
            if (op == side_effect_op::Construct)
            {
#if defined(__cpp_exceptions)
                throw std::runtime_error("ctor");
#endif
            }
        };

        ub_var t;
        EXPECT_THAT(t.has_value(), false);
        EXPECT_THAT(t.valueless_by_exception(), false);

#if defined(__cpp_exceptions)
        EXPECT_THROW(sink(t.emplace<MyCopyableAndMovable>('Y', throwing_side_effects)), std::runtime_error);

        EXPECT_THAT(t.has_value(), false);
        EXPECT_THAT(t.valueless_by_exception(), true);
        EXPECT_THAT(stats.constructs, 1);
        EXPECT_THAT(stats.destructs, 0);
        t.reset();
        EXPECT_THAT(stats.ops, "@");
#else
        EXPECT_THAT(t.emplace<MyCopyableAndMovable>('Y', throwing_side_effects), NotNull());

        EXPECT_THAT(t.has_value(), true);
        EXPECT_THAT(t.valueless_by_exception(), false);
        EXPECT_THAT(stats.constructs, 1);
        EXPECT_THAT(stats.destructs, 0);
        t.reset();
        EXPECT_THAT(stats.ops, "@~");
#endif
    }
}

TEST_F(TestPmrUnboundedVariant, emplace_2_initializer_list)
{
    struct MyType : rtti_helper<type_id_type<13>>
    {
        std::size_t size_;
        int         number_;

        MyType(const std::initializer_list<char> chars, const int number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using ub_var = unbounded_variant<sizeof(MyType)>;

    ub_var src;
    EXPECT_THAT(src.emplace<MyType>({'A', 'B', 'C'}, 42), NotNull());

    const auto test = get<MyType>(src);
    EXPECT_THAT(test.size_, 3);
    EXPECT_THAT(test.number_, 42);
}

TEST_F(TestPmrUnboundedVariant, pmr_only_ctor)
{
    using ub_var = unbounded_variant<0 /*Footprint*/, true /*Copyable*/, true /*Movable*/, 1 /*Alignment*/, pmr>;

    ub_var dst{get_default_mr()};
    EXPECT_THAT(dst.has_value(), false);

    dst = ub_var{get_default_mr(), 'x'};
    EXPECT_THAT(dst.has_value(), true);
    EXPECT_THAT(get<char>(dst), 'x');

    dst = Empty{};
    EXPECT_THAT(dst.has_value(), true);

    ub_var dst2{get_default_mr()};
    dst2 = std::move(dst);
    EXPECT_THAT(dst2.has_value(), true);

    dst2 = ub_var{get_default_mr()};
    EXPECT_THAT(dst2.has_value(), false);

    const ub_var src_empty{get_mr()};
    ub_var       dst3{src_empty};
    EXPECT_THAT(dst3.has_value(), false);
    EXPECT_THAT(dst3.get_memory_resource(), get_mr());

    const ub_var dst4{std::move(dst3)};
    EXPECT_THAT(dst4.has_value(), false);
    EXPECT_THAT(dst4.get_memory_resource(), get_mr());
}

TEST_F(TestPmrUnboundedVariant, pmr_ctor_with_footprint)
{
    using ub_var = unbounded_variant<2 /*Footprint*/, true /*Copyable*/, true /*Movable*/, 2 /*Alignment*/, pmr>;

    ub_var dst{get_mr()};
    EXPECT_THAT(dst.has_value(), false);
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    dst = ub_var{get_mr(), 'x'};
    EXPECT_THAT(dst.has_value(), true);
    EXPECT_THAT(get<char>(dst), 'x');
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    dst = Empty{};
    EXPECT_THAT(dst.has_value(), true);
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    ub_var dst2{get_default_mr()};
    EXPECT_THAT(dst2.get_memory_resource(), get_default_mr());
    dst2 = std::move(dst);
    EXPECT_THAT(dst2.get_memory_resource(), get_mr());
    EXPECT_THAT(dst2.has_value(), true);

    dst2 = ub_var{get_default_mr()};
    EXPECT_THAT(dst2.has_value(), false);
    dst2.reset(get_mr());

    dst2 = std::uint16_t{0x147};
    EXPECT_THAT(dst2.has_value(), true);
    EXPECT_THAT(get<std::uint16_t>(dst2), 0x147);

    dst2 = int{-1};
    EXPECT_THAT(dst2.has_value(), true);
    EXPECT_THAT(get<int>(dst2), -1);

    ub_var dst3{std::move(dst2)};
    EXPECT_THAT(dst3.get_memory_resource(), get_mr());
    EXPECT_THAT(dst3.has_value(), true);
    EXPECT_THAT(get<int>(dst3), -1);

    dst3 = true;
    EXPECT_THAT(dst3.has_value(), true);
    EXPECT_THAT(get<bool>(dst3), true);

    const ub_var src_empty{get_mr()};
    ub_var       dst4{src_empty};
    EXPECT_THAT(dst4.has_value(), false);
    EXPECT_THAT(dst4.get_memory_resource(), get_mr());

    ub_var dst5{std::move(dst4)};
    EXPECT_THAT(dst5.has_value(), false);
    EXPECT_THAT(dst5.get_memory_resource(), get_mr());

    dst5 = ub_var{get_default_mr()};
    EXPECT_THAT(dst5.has_value(), false);
    EXPECT_THAT(dst5.get_memory_resource(), get_default_mr());
}

TEST_F(TestPmrUnboundedVariant, pmr_ctor_no_footprint)
{
    using ub_var = unbounded_variant<0 /*Footprint*/, true /*Copyable*/, true /*Movable*/, 2 /*Alignment*/, pmr>;

    ub_var dst{get_mr()};
    EXPECT_THAT(dst.has_value(), false);
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    dst = ub_var{get_mr(), 'x'};
    EXPECT_THAT(dst.has_value(), true);
    EXPECT_THAT(get<char>(dst), 'x');
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    dst = Empty{};
    EXPECT_THAT(dst.has_value(), true);
    EXPECT_THAT(dst.get_memory_resource(), get_mr());

    ub_var dst2{get_default_mr()};
    EXPECT_THAT(dst2.get_memory_resource(), get_default_mr());
    dst2 = std::move(dst);
    EXPECT_THAT(dst2.get_memory_resource(), get_mr());
    EXPECT_THAT(dst2.has_value(), true);

    dst2 = ub_var{get_default_mr()};
    EXPECT_THAT(dst2.has_value(), false);
    dst2.reset(get_mr());

    dst2 = std::uint16_t{0x147};
    EXPECT_THAT(dst2.has_value(), true);
    EXPECT_THAT(get<std::uint16_t>(dst2), 0x147);

    dst2 = int{-1};
    EXPECT_THAT(dst2.has_value(), true);
    EXPECT_THAT(get<int>(dst2), -1);

    ub_var dst3{std::move(dst2)};
    EXPECT_THAT(dst3.get_memory_resource(), get_mr());
    EXPECT_THAT(dst3.has_value(), true);
    EXPECT_THAT(get<int>(dst3), -1);

    dst3 = true;
    EXPECT_THAT(dst3.has_value(), true);
    EXPECT_THAT(get<bool>(dst3), true);

    const ub_var src_empty{get_mr()};
    ub_var       dst4{src_empty};
    EXPECT_THAT(dst4.has_value(), false);
    EXPECT_THAT(dst4.get_memory_resource(), get_mr());

    ub_var dst5{std::move(dst4)};
    EXPECT_THAT(dst5.has_value(), false);
    EXPECT_THAT(dst5.get_memory_resource(), get_mr());

    dst5 = ub_var{get_default_mr()};
    EXPECT_THAT(dst5.has_value(), false);
    EXPECT_THAT(dst5.get_memory_resource(), get_default_mr());
}

TEST_F(TestPmrUnboundedVariant, pmr_with_footprint_move_value_when_out_of_memory)
{
    using ub_var = unbounded_variant<2 /*Footprint*/, false /*Copyable*/, true /*Movable*/, 4 /*Alignment*/, pmr>;

    side_effect_stats stats;
    auto              side_effects = stats.make_side_effect_fn();

    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    ub_var dst{mr_mock.resource()};

    // No allocations are expected (b/c we have footprint of 2).
    dst = true;
    dst = std::uint16_t{42};

    // Assign a bigger (`std::uint32_t`) type value which requires more than 2 bytes.
    // Emulate that there is enough memory.
    {
        using big_type = std::uint32_t;

        EXPECT_CALL(mr_mock, do_allocate(sizeof(big_type), 4))
            .WillOnce(
                [this](std::size_t size_bytes, std::size_t alignment) { return mr_.allocate(size_bytes, alignment); });
        EXPECT_CALL(mr_mock, do_deallocate(_, sizeof(big_type), 4))
            .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
                mr_.deallocate(p, size_bytes, alignment);
            });

        dst = big_type{13};
        EXPECT_THAT(dst.has_value(), true);
        EXPECT_THAT(dst.valueless_by_exception(), false);
    }

    // Assign even bigger (`double`) type value which requires more than 2 bytes.
    // Emulate that there is no memory enough.
    {
        MyMovableOnly my_move_only{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        EXPECT_CALL(mr_mock, do_allocate(sizeof(MyMovableOnly), 4)).WillOnce(Return(nullptr));

#if defined(__cpp_exceptions)
        EXPECT_THROW(sink(dst = std::move(my_move_only)), std::bad_alloc);
#else
        dst = std::move(my_move_only);
#endif
        EXPECT_THAT(dst.has_value(), false);
        EXPECT_THAT(dst.valueless_by_exception(), true);
        EXPECT_THAT(stats.ops, "@");
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@~");
}

TEST_F(TestPmrUnboundedVariant, pmr_with_footprint_copy_value_when_out_of_memory)
{
    const auto Alignment = alignof(std::max_align_t);
    using ub_var         = unbounded_variant<2 /*Footprint*/, true /*Copyable*/, false /*Movable*/, Alignment, pmr>;

    side_effect_stats stats;
    auto              side_effects = stats.make_side_effect_fn();

    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    ub_var dst{mr_mock.resource()};

    // No allocations are expected (b/c we have footprint of 2).
    dst = true;
    dst = std::uint16_t{42};

    // Assign a bigger (`MyCopyableOnly`) type value which requires more than 2 bytes.
    // Emulate that there is enough memory.
    {
        const MyCopyableOnly my_copy_only{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        EXPECT_CALL(mr_mock, do_allocate(sizeof(MyCopyableOnly), Alignment))
            .WillOnce(
                [this](std::size_t size_bytes, std::size_t alignment) { return mr_.allocate(size_bytes, alignment); });
        EXPECT_CALL(mr_mock, do_deallocate(_, sizeof(MyCopyableOnly), Alignment))
            .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
                mr_.deallocate(p, size_bytes, alignment);
            });

        dst = my_copy_only;
        EXPECT_THAT(stats.ops, "@C");

        dst.reset();
        EXPECT_THAT(stats.ops, "@C~");
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@C~~");

    // Emulate that there is no memory enough.
    {
        dst = true;
        stats.reset();

        MyCopyableOnly my_copy_only{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        EXPECT_CALL(mr_mock, do_allocate(sizeof(MyCopyableOnly), Alignment)).WillOnce(Return(nullptr));

#if defined(__cpp_exceptions)
        EXPECT_THROW(sink(dst = my_copy_only), std::bad_alloc);
#else
        dst = my_copy_only;
#endif
        EXPECT_THAT(dst.has_value(), false);
        EXPECT_THAT(dst.valueless_by_exception(), true);
        EXPECT_THAT(stats.ops, "@");
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@~");
}

TEST_F(TestPmrUnboundedVariant, pmr_no_footprint_move_value_when_out_of_memory)
{
    const auto Alignment = alignof(std::max_align_t);
    using ub_var         = unbounded_variant<0 /*Footprint*/, false /*Copyable*/, true /*Movable*/, Alignment, pmr>;

    side_effect_stats stats;
    auto              side_effects = stats.make_side_effect_fn();

    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    ub_var dst{mr_mock.resource()};

    // Emulate that there is no memory enough.
    {
        MyMovableOnly my_move_only{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        EXPECT_CALL(mr_mock, do_allocate(sizeof(MyMovableOnly), Alignment)).WillOnce(Return(nullptr));

#if defined(__cpp_exceptions)
        EXPECT_THROW(sink(dst = std::move(my_move_only)), std::bad_alloc);
#else
        dst = std::move(my_move_only);
#endif
        EXPECT_THAT(dst.has_value(), false);
        EXPECT_THAT(dst.valueless_by_exception(), true);
        EXPECT_THAT(stats.ops, "@");
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@~");
}

TEST_F(TestPmrUnboundedVariant, pmr_no_footprint_copy_value_when_out_of_memory)
{
    const auto Alignment = alignof(std::max_align_t);
    using ub_var         = unbounded_variant<0 /*Footprint*/, true /*Copyable*/, false /*Movable*/, Alignment, pmr>;

    side_effect_stats stats;
    auto              side_effects = stats.make_side_effect_fn();

    StrictMock<cetlvast::MemoryResourceMock> mr_mock{};

    EXPECT_CALL(mr_mock, do_allocate(sizeof(bool), Alignment))
        .WillOnce(
            [this](std::size_t size_bytes, std::size_t alignment) { return mr_.allocate(size_bytes, alignment); });
    EXPECT_CALL(mr_mock, do_deallocate(_, sizeof(bool), Alignment))
        .WillOnce([this](void* p, std::size_t size_bytes, std::size_t alignment) {
            mr_.deallocate(p, size_bytes, alignment);
        });
    ub_var dst{mr_mock.resource(), true};

    // Emulate that there is no memory enough.
    {
        MyCopyableOnly my_copy_only{'X', side_effects};
        EXPECT_THAT(stats.ops, "@");

        EXPECT_CALL(mr_mock, do_allocate(sizeof(MyCopyableOnly), Alignment)).WillOnce(Return(nullptr));

#if defined(__cpp_exceptions)
        EXPECT_THROW(sink(dst = my_copy_only), std::bad_alloc);
#else
        dst = my_copy_only;
#endif
        EXPECT_THAT(dst.has_value(), false);
        EXPECT_THAT(dst.valueless_by_exception(), true);
        EXPECT_THAT(stats.ops, "@");

        dst.reset();
        EXPECT_THAT(dst.has_value(), false);
        EXPECT_THAT(dst.valueless_by_exception(), false);
        EXPECT_THAT(stats.ops, "@");
    }
    EXPECT_THAT(stats.constructs, stats.destructs);
    EXPECT_THAT(stats.ops, "@~");
}

TEST_F(TestPmrUnboundedVariant, pmr_swap_copyable)
{
    using test   = MyCopyableOnly;
    using ub_var = unbounded_variant<0, true, false, alignof(std::max_align_t), pmr>;

    ub_var empty{get_default_mr()};
    ub_var a{get_default_mr(), in_place_type_t<test>{}, 'A'};
    ub_var b{get_default_mr(), in_place_type_t<test>{}, 'B'};

    // Self swap
    a.swap(a);
    EXPECT_THAT(get<const test&>(a).payload_, 'A');
    EXPECT_THAT(get_if<MyCopyableAndMovable>(&a), IsNull());

    a.swap(b);
    EXPECT_THAT(get<test&>(a).payload_, 'B');
    EXPECT_THAT(get<test&>(b).payload_, 'A');

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_THAT(get<test&>(empty).payload_, 'B');

    empty.swap(a);
    EXPECT_FALSE(empty.has_value());
    EXPECT_THAT(get<test&>(a).payload_, 'B');

    ub_var another_empty{get_default_mr()};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
}

TEST_F(TestPmrUnboundedVariant, pmr_swap_movable)
{
    using test   = MyMovableOnly;
    using ub_var = unbounded_variant<sizeof(test), false, true, alignof(std::max_align_t), pmr>;

    ub_var empty{get_mr()};
    ub_var a{get_mr(), in_place_type_t<test>{}, 'A'};
    EXPECT_THAT(a.get_memory_resource(), get_mr());
    ub_var b{get_default_mr(), in_place_type_t<test>{}, 'B'};
    EXPECT_THAT(b.get_memory_resource(), get_default_mr());

    // Self swap
    a.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_THAT(get<const test&>(a).payload_, 'A');
    EXPECT_THAT(a.get_memory_resource(), get_mr());

    a.swap(b);
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_FALSE(get<test&>(b).moved_);
    EXPECT_THAT(get<test&>(a).payload_, 'B');
    EXPECT_THAT(get<test&>(b).payload_, 'A');
    EXPECT_THAT(a.get_memory_resource(), get_default_mr());
    EXPECT_THAT(b.get_memory_resource(), get_mr());

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_TRUE(empty.has_value());
    EXPECT_FALSE(get<test&>(empty).moved_);
    EXPECT_THAT(get<test&>(empty).payload_, 'B');
    EXPECT_THAT(a.get_memory_resource(), get_mr());
    EXPECT_THAT(empty.get_memory_resource(), get_default_mr());

    empty.swap(a);
    EXPECT_TRUE(a.has_value());
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(get<test&>(a).moved_);
    EXPECT_THAT(get<test&>(a).payload_, 'B');
    EXPECT_THAT(empty.get_memory_resource(), get_mr());
    EXPECT_THAT(a.get_memory_resource(), get_default_mr());

    ub_var another_empty{get_default_mr()};
    empty.swap(another_empty);
    EXPECT_FALSE(empty.has_value());
    EXPECT_FALSE(another_empty.has_value());
    EXPECT_THAT(another_empty.get_memory_resource(), get_mr());
    EXPECT_THAT(empty.get_memory_resource(), get_default_mr());

    const ub_var ub_vec{get_mr(), in_place_type_t<std::vector<char>>{}, {'A', 'B', 'C'}};
    EXPECT_THAT(ub_vec.get_memory_resource(), get_mr());
    EXPECT_THAT(get<const std::vector<char>&>(ub_vec), testing::ElementsAre('A', 'B', 'C'));
}

TEST_F(TestPmrUnboundedVariant, pmr_reset_memory_resource)
{
    using test   = MyMovableOnly;
    using ub_var = unbounded_variant<sizeof(test), false, true, alignof(std::max_align_t), pmr>;

    ub_var a{get_mr(), in_place_type_t<test>{}, 'A'};
    EXPECT_TRUE(a.has_value());
    EXPECT_THAT(a.get_memory_resource(), get_mr());

    a.reset(get_default_mr());
    EXPECT_FALSE(a.has_value());
    EXPECT_THAT(a.get_memory_resource(), get_default_mr());
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
constexpr type_id type_id_value<uint16_t> = {7};

template <>
constexpr type_id type_id_value<std::unique_ptr<MyCopyableAndMovable>> =
    {0xB3, 0xB8, 0x4E, 0xC1, 0x1F, 0xE4, 0x49, 0x35, 0x9E, 0xC9, 0x1A, 0x77, 0x7B, 0x82, 0x53, 0x25};

template <>
constexpr type_id type_id_value<std::complex<double>> = {8};

template <>
constexpr type_id type_id_value<std::function<const char*()>> = {9};

template <>
constexpr type_id type_id_value<Empty> = {10};

template <>
constexpr type_id type_id_value<std::uint32_t> = {11};

template <>
constexpr type_id type_id_value<std::vector<char>> = {12};

}  // namespace cetl

// NOLINTEND(*-use-after-move)
