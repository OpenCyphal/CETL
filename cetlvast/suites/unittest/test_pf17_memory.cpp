/// @file
/// Unit tests for cetl/pf17/memory.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/memory.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <forward_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <type_traits>

namespace
{
struct Polyfill
{
    template <typename InputIt, typename ForwardIt>
    static ForwardIt move(InputIt first, InputIt last, ForwardIt destination)
    {
        return cetl::pf17::uninitialized_move(first, last, destination);
    }
};

struct Facade
{
    template <typename InputIt, typename ForwardIt>
    static ForwardIt move(InputIt first, InputIt last, ForwardIt destination)
    {
        return cetl::uninitialized_move(first, last, destination);
    }
};

#if (__cplusplus >= CETL_CPP_STANDARD_17)
struct Standard
{
    template <typename InputIt, typename ForwardIt>
    static ForwardIt move(InputIt first, InputIt last, ForwardIt destination)
    {
        return std::uninitialized_move(first, last, destination);
    }
};
using TestTypes = testing::Types<Polyfill, Facade, Standard>;
#else
using TestTypes = testing::Types<Polyfill, Facade>;
#endif

template <typename Algorithm>
class TestUninitializedMove : public testing::Test
{};

TYPED_TEST_SUITE(TestUninitializedMove, TestTypes, );

/// Owns only raw storage. Each test is responsible for checking and ending object lifetimes before deallocation.
template <typename T, std::size_t Size>
class Storage
{
public:
    Storage()
        : data_(allocator_.allocate(Size))
    {
    }
    Storage(const Storage&)            = delete;
    Storage(Storage&&)                 = delete;
    Storage& operator=(const Storage&) = delete;
    Storage& operator=(Storage&&)      = delete;
    ~Storage()
    {
        allocator_.deallocate(data_, Size);
    }
    T* data() const noexcept
    {
        return data_;
    }

private:
    std::allocator<T> allocator_;
    T*                data_;
};

template <typename T>
class ForwardIterator
{
public:
    using iterator_category = std::forward_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T&;

    ForwardIterator() noexcept = default;
    explicit ForwardIterator(T* ptr) noexcept
        : ptr_(ptr)
    {
    }
    T& operator*() const noexcept
    {
        return *ptr_;
    }
    T* operator->() const noexcept
    {
        return ptr_;
    }
    ForwardIterator& operator++() noexcept
    {
        ++ptr_;
        return *this;
    }
    ForwardIterator operator++(int) noexcept
    {
        const auto previous = *this;
        ++*this;
        return previous;
    }
    bool operator==(const ForwardIterator& rhs) const noexcept
    {
        return ptr_ == rhs.ptr_;
    }
    bool operator!=(const ForwardIterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }

private:
    T* ptr_ = nullptr;
};

struct LifetimeState
{
    std::array<std::size_t, 3> constructed{};
    std::array<std::size_t, 3> destroyed{};
    std::size_t                moves             = 0;
    std::size_t                copies            = 0;
    std::size_t                sources_destroyed = 0;
    std::size_t                throw_on          = 3;
};

struct Failure
{
    const LifetimeState* state;
};

struct Tracked
{
    LifetimeState&    state;
    const std::size_t id;
    const bool        destination;

    Tracked(LifetimeState& state_in, const std::size_t id_in) noexcept
        : state(state_in)
        , id(id_in)
        , destination(false)
    {
    }
    Tracked(const Tracked& rhs) noexcept
        : state(rhs.state)
        , id(rhs.id)
        , destination(true)
    {
        ++state.copies;
        ++state.constructed.at(id);
    }
    Tracked(Tracked&& rhs)  // NOLINT(*-noexcept-move-constructor)
        : state(rhs.state)
        , id(rhs.id)
        , destination(true)
    {
        ++state.moves;
#if defined(__cpp_exceptions)
        if (id == state.throw_on)
        {
            throw Failure{&state};
        }
#endif
        ++state.constructed.at(id);
    }
    Tracked& operator=(const Tracked&) = delete;
    Tracked& operator=(Tracked&&)      = delete;
    ~Tracked()
    {
        if (destination)
        {
            ++state.destroyed.at(id);
        }
        else
        {
            ++state.sources_destroyed;
        }
    }
};

TYPED_TEST(TestUninitializedMove, EmptyRange)
{
    LifetimeState       state;
    Tracked             source{state, 0};
    Storage<Tracked, 1> storage;
    EXPECT_EQ(TypeParam::move(&source, &source, storage.data()), storage.data());
    EXPECT_EQ(state.moves, 0U);
    EXPECT_EQ(state.copies, 0U);
    EXPECT_EQ(state.destroyed, (std::array<std::size_t, 3>{0, 0, 0}));
}

TYPED_TEST(TestUninitializedMove, SingleElement)
{
    int             source = 42;
    Storage<int, 1> storage;
    EXPECT_EQ(TypeParam::move(&source, &source + 1, storage.data()), storage.data() + 1);
    EXPECT_EQ(*storage.data(), 42);
}

TYPED_TEST(TestUninitializedMove, MovesEvenWhenCopyIsNoexcept)
{
    static_assert(std::is_nothrow_copy_constructible<Tracked>::value, "Copy must be a nonthrowing alternative");
    static_assert(!std::is_nothrow_move_constructible<Tracked>::value, "Move must be potentially throwing");
    LifetimeState       state;
    Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
    Storage<Tracked, 3> storage;
    auto* const         end = TypeParam::move(std::begin(source), std::end(source), storage.data());
    EXPECT_EQ(end, storage.data() + 3);
    EXPECT_EQ(state.moves, 3U);
    EXPECT_EQ(state.copies, 0U);
    EXPECT_EQ(state.constructed, (std::array<std::size_t, 3>{1, 1, 1}));
    EXPECT_EQ(state.destroyed, (std::array<std::size_t, 3>{0, 0, 0}));
    EXPECT_EQ(state.sources_destroyed, 0U);
    for (std::size_t i = 0; i < 3; ++i)
    {
        EXPECT_EQ(storage.data()[i].id, i);
        storage.data()[i].~Tracked();
    }
    EXPECT_EQ(state.destroyed, state.constructed);
}

TYPED_TEST(TestUninitializedMove, MoveOnlyFromNoncontiguousSource)
{
    using Value = std::unique_ptr<int>;
    std::forward_list<Value> source;
    source.emplace_front(new int{30});
    source.emplace_front(new int{20});
    source.emplace_front(new int{10});
    Storage<Value, 3> storage;
    EXPECT_EQ(TypeParam::move(source.begin(), source.end(), storage.data()), storage.data() + 3);
    for (std::size_t i = 0; i < 3; ++i)
    {
        ASSERT_NE(storage.data()[i], nullptr);
        EXPECT_EQ(*storage.data()[i], static_cast<int>((i + 1) * 10));
        storage.data()[i].~Value();
    }
}

struct CopyOnly
{
    explicit CopyOnly(const int value_in) noexcept
        : value(value_in)
    {
    }
    CopyOnly(const CopyOnly& rhs) noexcept
        : value(rhs.value)
    {
    }
    CopyOnly& operator=(const CopyOnly&) = delete;
    int       value;
};

TYPED_TEST(TestUninitializedMove, CopyOnly)
{
    CopyOnly             source[] = {CopyOnly{10}, CopyOnly{20}};
    Storage<CopyOnly, 2> storage;
    EXPECT_EQ(TypeParam::move(std::begin(source), std::end(source), storage.data()), storage.data() + 2);
    EXPECT_EQ(storage.data()[0].value, 10);
    EXPECT_EQ(storage.data()[1].value, 20);
    storage.data()[0].~CopyOnly();
    storage.data()[1].~CopyOnly();
}

TYPED_TEST(TestUninitializedMove, SinglePassInputAndForwardDestination)
{
    std::istringstream input{"10 20 30"};
    Storage<int, 3>    storage;
    const auto         end = TypeParam::move(std::istream_iterator<int>{input},
                                             std::istream_iterator<int>{},
                                             ForwardIterator<int>{storage.data()});
    EXPECT_EQ(end, ForwardIterator<int>{storage.data() + 3});
    EXPECT_EQ(storage.data()[0], 10);
    EXPECT_EQ(storage.data()[1], 20);
    EXPECT_EQ(storage.data()[2], 30);
}

struct AddressSensitive
{
    int value;

    AddressSensitive*       operator&()                      = delete;
    const AddressSensitive* operator&() const                = delete;
    static void*            operator new(std::size_t, void*) = delete;
};

TYPED_TEST(TestUninitializedMove, BypassesAddressOfAndClassPlacementNew)
{
    AddressSensitive             source[] = {{10}, {20}};
    Storage<AddressSensitive, 2> storage;
    EXPECT_EQ(TypeParam::move(std::begin(source), std::end(source), storage.data()), storage.data() + 2);
    EXPECT_EQ(storage.data()[0].value, 10);
    EXPECT_EQ(storage.data()[1].value, 20);
    storage.data()[0].~AddressSensitive();
    storage.data()[1].~AddressSensitive();
}

#if defined(__cpp_exceptions)
TYPED_TEST(TestUninitializedMove, ConstructionFailuresCleanUpExactlyTheConstructedPrefix)
{
    for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        state.throw_on = fail_at;
        try
        {
            (void) TypeParam::move(std::begin(source), std::end(source), ForwardIterator<Tracked>{storage.data()});
            FAIL() << "Expected construction to throw";
        } catch (const Failure& failure)
        {
            EXPECT_EQ(failure.state, &state);
        }
        EXPECT_EQ(state.moves, fail_at + 1);
        EXPECT_EQ(state.copies, 0U);
        EXPECT_EQ(state.sources_destroyed, 0U);
        for (std::size_t i = 0; i < 3; ++i)
        {
            EXPECT_EQ(state.constructed[i], i < fail_at ? 1U : 0U);
            EXPECT_EQ(state.destroyed[i], state.constructed[i]);
        }
    }
}

enum class IteratorOperation
{
    Dereference,
    Increment,
    Comparison
};

/// An input iterator whose permitted throwing operations can fail after at least one successful construction.
class ThrowingInputIterator
{
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = Tracked;
    using difference_type   = std::ptrdiff_t;
    using pointer           = Tracked*;
    using reference         = Tracked&;

    ThrowingInputIterator(Tracked* ptr, const IteratorOperation operation, LifetimeState& state) noexcept
        : ptr_(ptr)
        , operation_(operation)
        , state_(&state)
    {
    }
    Tracked& operator*() const
    {
        fail_if(IteratorOperation::Dereference);
        return *ptr_;
    }
    Tracked* operator->() const
    {
        return std::addressof(**this);
    }
    ThrowingInputIterator& operator++()
    {
        fail_if(IteratorOperation::Increment);
        ++ptr_;
        return *this;
    }
    ThrowingInputIterator operator++(int)
    {
        const auto previous = *this;
        ++*this;
        return previous;
    }
    bool operator==(const ThrowingInputIterator& rhs) const
    {
        fail_if(IteratorOperation::Comparison);
        return ptr_ == rhs.ptr_;
    }
    bool operator!=(const ThrowingInputIterator& rhs) const
    {
        return !(*this == rhs);
    }

private:
    void fail_if(const IteratorOperation operation) const
    {
        if ((operation == operation_) && (state_->moves == 1))
        {
            throw Failure{state_};
        }
    }

    Tracked*          ptr_;
    IteratorOperation operation_;
    LifetimeState*    state_;
};

TEST(TestPf17Memory, InputIteratorFailuresCleanUpTheMostRecentConstruction)
{
    for (const auto operation :
         {IteratorOperation::Dereference, IteratorOperation::Increment, IteratorOperation::Comparison})
    {
        SCOPED_TRACE(static_cast<int>(operation));
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        try
        {
            (void) cetl::pf17::uninitialized_move(ThrowingInputIterator{std::begin(source), operation, state},
                                                  ThrowingInputIterator{std::end(source), operation, state},
                                                  ForwardIterator<Tracked>{storage.data()});
            FAIL() << "Expected an input iterator operation to throw";
        } catch (const Failure& failure)
        {
            EXPECT_EQ(failure.state, &state);
        }
        EXPECT_EQ(state.constructed, (std::array<std::size_t, 3>{1, 0, 0}));
        EXPECT_EQ(state.destroyed, state.constructed);
        EXPECT_EQ(state.sources_destroyed, 0U);
    }
}
#endif

}  // namespace
