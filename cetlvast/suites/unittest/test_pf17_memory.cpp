/// @file
/// Unit tests for cetl/pf17/memory.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/memory.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include "test_pf17_memory_exception.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <exception>
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
        if (id == state.throw_on)
        {
#if defined(__cpp_exceptions)
            throw Failure{&state};
#else
            cetlvast::pf17_memory_test::throw_exception();
#endif
        }
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

TYPED_TEST(TestUninitializedMove, ConstSourceIsCopied)
{
    LifetimeState       state;
    const Tracked       source[] = {{state, 0}, {state, 1}};
    Storage<Tracked, 2> storage;
    EXPECT_EQ(TypeParam::move(std::begin(source), std::end(source), storage.data()), storage.data() + 2);
    EXPECT_EQ(state.moves, 0U);
    EXPECT_EQ(state.copies, 2U);
    EXPECT_EQ(state.sources_destroyed, 0U);
    storage.data()[0].~Tracked();
    storage.data()[1].~Tracked();
    EXPECT_EQ(state.destroyed, state.constructed);
}

struct GeneratedState
{
    std::size_t                copies   = 0;
    std::size_t                moves    = 0;
    std::size_t                live     = 0;
    int                        throw_on = 3;
    std::array<std::size_t, 3> constructed{};
    std::array<std::size_t, 3> destroyed{};
};

struct GeneratedFailure
{
    const GeneratedState* state;
};

struct GeneratedValue
{
    GeneratedState& state;
    const int       value;

    GeneratedValue(GeneratedState& state_in, const int value_in)
        : state(state_in)
        , value(value_in)
    {
        if (value == state.throw_on)
        {
#if defined(__cpp_exceptions)
            throw GeneratedFailure{&state};
#else
            cetlvast::pf17_memory_test::throw_exception();
#endif
        }
        ++state.constructed.at(static_cast<std::size_t>(value));
        ++state.live;
    }
    GeneratedValue(const GeneratedValue& rhs) noexcept
        : GeneratedValue(rhs.state, rhs.value)
    {
        ++state.copies;
    }
    GeneratedValue(GeneratedValue&& rhs) noexcept
        : GeneratedValue(rhs.state, rhs.value)
    {
        ++state.moves;
    }
    ~GeneratedValue()
    {
        ++state.destroyed.at(static_cast<std::size_t>(value));
        --state.live;
    }
};

/// Dereferencing generates a prvalue instead of referring to an existing source object.
template <typename T>
class GeneratingIterator
{
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = T*;
    using reference         = T;

    GeneratingIterator(GeneratedState& state, const int index) noexcept
        : state_(&state)
        , index_(index)
    {
    }
    T operator*() const
    {
        return T{*state_, index_};
    }
    GeneratingIterator& operator++() noexcept
    {
        ++index_;
        return *this;
    }
    GeneratingIterator operator++(int) noexcept
    {
        const auto previous = *this;
        ++*this;
        return previous;
    }
    bool operator==(const GeneratingIterator& rhs) const noexcept
    {
        return state_ == rhs.state_ && index_ == rhs.index_;
    }
    bool operator!=(const GeneratingIterator& rhs) const noexcept
    {
        return !(*this == rhs);
    }

private:
    GeneratedState* state_;
    int             index_;
};

// Test the direct polyfill: the standard library selected by the facade may not yet implement LWG 3918.
TEST(TestPf17Memory, PrvaluesConstructDirectlyInDestination)
{
    GeneratedState             state;
    Storage<GeneratedValue, 3> storage;
    const auto                 end = cetl::pf17::uninitialized_move(GeneratingIterator<GeneratedValue>{state, 0},
                                                    GeneratingIterator<GeneratedValue>{state, 3},
                                                    storage.data());
    EXPECT_EQ(end, storage.data() + 3);
    EXPECT_EQ(state.live, 3U);
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    EXPECT_EQ(state.copies, 0U);
    EXPECT_EQ(state.moves, 0U);
#endif
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(storage.data()[i].value, i);
        storage.data()[i].~GeneratedValue();
    }
    EXPECT_EQ(state.live, 0U);
}

TYPED_TEST(TestUninitializedMove, XvaluesStillMove)
{
    LifetimeState       state;
    Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
    Storage<Tracked, 3> storage;
    EXPECT_EQ(TypeParam::move(std::make_move_iterator(std::begin(source)),
                              std::make_move_iterator(std::end(source)),
                              storage.data()),
              storage.data() + 3);
    EXPECT_EQ(state.moves, 3U);
    EXPECT_EQ(state.copies, 0U);
    for (std::size_t i = 0; i < 3; ++i)
    {
        storage.data()[i].~Tracked();
    }
    EXPECT_EQ(state.destroyed, state.constructed);
}

#if (__cplusplus >= CETL_CPP_STANDARD_17)
struct ImmovableGeneratedValue : GeneratedValue
{
    using GeneratedValue::GeneratedValue;
    ImmovableGeneratedValue(const ImmovableGeneratedValue&) = delete;
    ImmovableGeneratedValue(ImmovableGeneratedValue&&)      = delete;
};

TEST(TestPf17Memory, PrvaluesDoNotRequireCopyOrMoveConstruction)
{
    GeneratedState                      state;
    Storage<ImmovableGeneratedValue, 3> storage;
    const auto end = cetl::pf17::uninitialized_move(GeneratingIterator<ImmovableGeneratedValue>{state, 0},
                                                    GeneratingIterator<ImmovableGeneratedValue>{state, 3},
                                                    storage.data());
    EXPECT_EQ(end, storage.data() + 3);
    EXPECT_EQ(state.live, 3U);
    EXPECT_EQ(state.copies, 0U);
    EXPECT_EQ(state.moves, 0U);
    for (int i = 0; i < 3; ++i)
    {
        EXPECT_EQ(storage.data()[i].value, i);
        storage.data()[i].~ImmovableGeneratedValue();
    }
    EXPECT_EQ(state.live, 0U);
}
#endif

/// Models the allocator adapter needed by containers without giving the public algorithm allocator semantics.
struct AllocatorOperations
{
    std::allocator<Tracked> allocator;
    std::size_t             constructions = 0;
    std::size_t             destructions  = 0;

    void construct(Tracked* address, Tracked& source)
    {
        ++constructions;
        std::allocator_traits<decltype(allocator)>::construct(allocator, address, std::move(source));
    }
    void destroy(Tracked* address) noexcept
    {
        ++destructions;
        std::allocator_traits<decltype(allocator)>::destroy(allocator, address);
    }
};

TEST(TestPf17Memory, InternalCallbacksLeaveSuccessfulObjectsAlive)
{
    LifetimeState       state;
    Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
    Storage<Tracked, 3> storage;
    AllocatorOperations operations;
    const auto          end = cetl::pf17::detail::mem::uninitialized_construct(
        std::begin(source),
        std::end(source),
        ForwardIterator<Tracked>{storage.data()},
        [&operations](Tracked* address, Tracked*& source_it) { operations.construct(address, *source_it); },
        [&operations](Tracked* address) { operations.destroy(address); });
    EXPECT_EQ(end, ForwardIterator<Tracked>{storage.data() + 3});
    EXPECT_EQ(operations.constructions, 3U);
    EXPECT_EQ(operations.destructions, 0U);
    EXPECT_EQ(state.constructed, (std::array<std::size_t, 3>{1, 1, 1}));
    EXPECT_EQ(state.destroyed, (std::array<std::size_t, 3>{0, 0, 0}));
    for (std::size_t i = 0; i < 3; ++i)
    {
        operations.destroy(storage.data() + i);
    }
    EXPECT_EQ(state.destroyed, state.constructed);
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
TEST(TestPf17Memory, InternalCallbacksRollBackThroughAllocator)
{
    LifetimeState       state;
    Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
    Storage<Tracked, 3> storage;
    AllocatorOperations operations;
    state.throw_on = 2;
    try
    {
        (void) cetl::pf17::detail::mem::uninitialized_construct(
            std::begin(source),
            std::end(source),
            ForwardIterator<Tracked>{storage.data()},
            [&operations](Tracked* address, Tracked*& source_it) { operations.construct(address, *source_it); },
            [&operations](Tracked* address) { operations.destroy(address); });
        FAIL() << "Expected allocator construction to throw";
    } catch (const Failure& failure)
    {
        EXPECT_EQ(failure.state, &state);
    }
    EXPECT_EQ(operations.constructions, 3U);
    EXPECT_EQ(operations.destructions, 2U);
    EXPECT_EQ(state.constructed, (std::array<std::size_t, 3>{1, 1, 0}));
    EXPECT_EQ(state.destroyed, state.constructed);
    EXPECT_EQ(state.sources_destroyed, 0U);
}

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

TYPED_TEST(TestUninitializedMove, XvalueConstructionFailuresCleanUpExactlyTheConstructedPrefix)
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
            (void) TypeParam::move(std::make_move_iterator(std::begin(source)),
                                   std::make_move_iterator(std::end(source)),
                                   storage.data());
            FAIL() << "Expected move construction to throw";
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

TEST(TestPf17Memory, PrvalueConstructionFailuresCleanUpExactlyTheConstructedPrefix)
{
    for (int fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        GeneratedState             state;
        Storage<GeneratedValue, 3> storage;
        state.throw_on = fail_at;
        try
        {
            (void) cetl::pf17::uninitialized_move(GeneratingIterator<GeneratedValue>{state, 0},
                                                  GeneratingIterator<GeneratedValue>{state, 3},
                                                  storage.data());
            FAIL() << "Expected prvalue construction to throw";
        } catch (const GeneratedFailure& failure)
        {
            EXPECT_EQ(failure.state, &state);
        }
        EXPECT_EQ(state.live, 0U);
        for (int i = 0; i < 3; ++i)
        {
            const auto index = static_cast<std::size_t>(i);
            if (i < fail_at)
            {
                EXPECT_GT(state.constructed[index], 0U);
            }
            else
            {
                EXPECT_EQ(state.constructed[index], 0U);
            }
            EXPECT_EQ(state.destroyed[index], state.constructed[index]);
#    if (__cplusplus >= CETL_CPP_STANDARD_17)
            EXPECT_EQ(state.constructed[index], i < fail_at ? 1U : 0U);
#    endif
        }
#    if (__cplusplus >= CETL_CPP_STANDARD_17)
        EXPECT_EQ(state.copies, 0U);
        EXPECT_EQ(state.moves, 0U);
#    endif
    }
}

TEST(TestPf17Memory, StreamExtractionFailurePropagates)
{
    std::istringstream input{"10 invalid"};
    input.exceptions(std::ios::failbit | std::ios::badbit);
    const std::istream_iterator<int> first{input};
    Storage<int, 2>                  storage;
    ASSERT_EQ(*first, 10);
    EXPECT_THROW((void) cetl::pf17::uninitialized_move(first,
                                                       std::istream_iterator<int>{},
                                                       ForwardIterator<int>{storage.data()}),
                 std::ios_base::failure);
    EXPECT_TRUE(input.fail());
}

#endif

enum class IteratorOperation
{
    None,
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
#if defined(__cpp_exceptions)
            throw Failure{state_};
#else
            cetlvast::pf17_memory_test::throw_exception();
#endif
        }
    }

    Tracked*          ptr_;
    IteratorOperation operation_;
    LifetimeState*    state_;
};

TYPED_TEST(TestUninitializedMove, ForwardDestinationHandlesEmptyAndCompleteRanges)
{
    for (const std::size_t size : {0U, 3U})
    {
        SCOPED_TRACE(size);
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        const auto          end = TypeParam::move(source, source + size, ForwardIterator<Tracked>{storage.data()});
        EXPECT_EQ(end, ForwardIterator<Tracked>{storage.data() + size});
        EXPECT_EQ(state.moves, size);
        EXPECT_EQ(state.copies, 0U);
        EXPECT_EQ(state.sources_destroyed, 0U);
        EXPECT_EQ(state.destroyed, (std::array<std::size_t, 3>{0, 0, 0}));
        for (std::size_t i = 0; i < 3; ++i)
        {
            EXPECT_EQ(state.constructed[i], i < size ? 1U : 0U);
            if (i < size)
            {
                EXPECT_EQ(storage.data()[i].id, i);
                storage.data()[i].~Tracked();
            }
        }
        EXPECT_EQ(state.destroyed, state.constructed);
    }
}

TYPED_TEST(TestUninitializedMove, InputIteratorWithoutFailureHandlesEmptyAndCompleteRanges)
{
    for (const std::size_t size : {0U, 3U})
    {
        SCOPED_TRACE(size);
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        const auto          end = TypeParam::move(ThrowingInputIterator{source, IteratorOperation::None, state},
                                         ThrowingInputIterator{source + size, IteratorOperation::None, state},
                                         ForwardIterator<Tracked>{storage.data()});
        EXPECT_EQ(end, ForwardIterator<Tracked>{storage.data() + size});
        EXPECT_EQ(state.moves, size);
        EXPECT_EQ(state.copies, 0U);
        EXPECT_EQ(state.sources_destroyed, 0U);
        EXPECT_EQ(state.destroyed, (std::array<std::size_t, 3>{0, 0, 0}));
        for (std::size_t i = 0; i < 3; ++i)
        {
            EXPECT_EQ(state.constructed[i], i < size ? 1U : 0U);
            if (i < size)
            {
                EXPECT_EQ(storage.data()[i].id, i);
                storage.data()[i].~Tracked();
            }
        }
        EXPECT_EQ(state.destroyed, state.constructed);
    }
}

#if defined(__cpp_exceptions)
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

#if !defined(__cpp_exceptions) && GTEST_HAS_DEATH_TEST
// An uncaught exception must reach std::terminate in this exception-disabled test executable. This does not
// promise rollback or define exception propagation through arbitrary mixtures of compiler exception settings.
constexpr int exception_termination_exit_code = 86;

[[noreturn]] void report_exception_termination() noexcept
{
    if (std::current_exception())
    {
        std::fputs("uninitialized_move terminated with an active exception\n", stderr);
        std::_Exit(exception_termination_exit_code);
    }
    std::_Exit(87);  // A direct call to terminate without an exception must not pass these tests.
}

template <typename Algorithm>
class TestUninitializedMoveDeathTest : public testing::Test
{};

TYPED_TEST_SUITE(TestUninitializedMoveDeathTest, TestTypes, );

TYPED_TEST(TestUninitializedMoveDeathTest, ConstructionExceptionTerminates)
{
    for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        state.throw_on = fail_at;
        EXPECT_EXIT(
            {
                std::set_terminate(report_exception_termination);
                (void) TypeParam::move(std::begin(source), std::end(source), ForwardIterator<Tracked>{storage.data()});
            },
            testing::ExitedWithCode(exception_termination_exit_code),
            "uninitialized_move terminated with an active exception");
    }
}

TYPED_TEST(TestUninitializedMoveDeathTest, InputIteratorExceptionTerminates)
{
    for (const auto operation :
         {IteratorOperation::Dereference, IteratorOperation::Increment, IteratorOperation::Comparison})
    {
        SCOPED_TRACE(static_cast<int>(operation));
        LifetimeState       state;
        Tracked             source[] = {{state, 0}, {state, 1}, {state, 2}};
        Storage<Tracked, 3> storage;
        EXPECT_EXIT(
            {
                std::set_terminate(report_exception_termination);
                (void) TypeParam::move(ThrowingInputIterator{std::begin(source), operation, state},
                                       ThrowingInputIterator{std::end(source), operation, state},
                                       ForwardIterator<Tracked>{storage.data()});
            },
            testing::ExitedWithCode(exception_termination_exit_code),
            "uninitialized_move terminated with an active exception");
    }
}
#endif

}  // namespace
