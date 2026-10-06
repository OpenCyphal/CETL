/// @file
/// Tests object lifetime and allocation ownership after partial VLA construction.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <map>
#include <set>
#include <memory>
#include <utility>

namespace
{
#if defined(__cpp_exceptions)
enum class Operation
{
    None,
    Copy,
    Move,
    Default,
    Read,
    Assign
};
struct Failure
{};

struct LifetimeState
{
    std::set<const void*> live;
    std::size_t           constructed = 0;
    std::size_t           destroyed   = 0;
    Operation             operation   = Operation::None;
    std::size_t           remaining   = 0;

    void arm(Operation op, std::size_t fail_at)
    {
        operation = op;
        remaining = fail_at;
    }
    void attempt(Operation op)
    {
        if (operation == op)
        {
            if (remaining == 0)
            {
                throw Failure{};
            }
            --remaining;
        }
    }
    void add(const void* pointer)
    {
        EXPECT_TRUE(live.insert(pointer).second);
        ++constructed;
    }
    void remove(const void* pointer)
    {
        EXPECT_EQ(live.erase(pointer), 1U);
        ++destroyed;
    }
    ~LifetimeState()
    {
        EXPECT_TRUE(live.empty());
        EXPECT_EQ(constructed, destroyed);
    }
};

LifetimeState* default_state = nullptr;

struct Value
{
    LifetimeState* state;
    int            value;
    Value(LifetimeState& owner, int v)
        : state(&owner)
        , value(v)
    {
        state->add(this);
    }
    Value()
        : state(default_state)
        , value(0)
    {
        state->attempt(Operation::Default);
        state->add(this);
    }
    Value(const Value& rhs)
        : state(rhs.state)
        , value(rhs.value)
    {
        state->attempt(Operation::Copy);
        state->add(this);
    }
    Value(Value&& rhs)
        : state(rhs.state)
        , value(rhs.value)
    {
        rhs.value = -1;
        state->attempt(Operation::Move);
        state->add(this);
    }
    Value& operator=(const Value& rhs)
    {
        state->attempt(Operation::Assign);
        value = rhs.value;
        return *this;
    }
    Value& operator=(Value&& rhs)
    {
        state->attempt(Operation::Assign);
        value     = rhs.value;
        rhs.value = -1;
        return *this;
    }
    ~Value()
    {
        state->remove(this);
    }
};

struct MoveOnly : Value
{
    using Value::Value;
    MoveOnly(const MoveOnly&)            = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& rhs)
        : Value(std::move(rhs))
    {
    }
    MoveOnly& operator=(MoveOnly&& rhs)
    {
        Value::operator=(std::move(rhs));
        return *this;
    }
};

struct AllocationState
{
    std::map<void*, std::size_t> allocations;
    std::set<void*>              objects;
    std::size_t                  allocated                    = 0;
    std::size_t                  deallocated                  = 0;
    bool                         fail                         = false;
    bool                         fail_construct               = false;
    std::size_t                  constructions_before_failure = 0;
    ~AllocationState()
    {
        EXPECT_TRUE(allocations.empty());
        EXPECT_TRUE(objects.empty());
        EXPECT_EQ(allocated, deallocated);
    }
};

template <typename T, bool Propagate = false>
struct Allocator
{
    using value_type                             = T;
    using is_always_equal                        = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;
    using propagate_on_container_copy_assignment = std::integral_constant<bool, Propagate>;
    template <typename U>
    struct rebind
    {
        using other = Allocator<U, Propagate>;
    };
    AllocationState* state;
    explicit Allocator(AllocationState& s) noexcept
        : state(&s)
    {
    }
    template <typename U>
    Allocator(const Allocator<U, Propagate>& rhs) noexcept
        : state(rhs.state)
    {
    }
    T* allocate(std::size_t count)
    {
        if (state->fail)
        {
            throw std::bad_alloc{};
        }
        T* result = std::allocator<T>{}.allocate(count);
        EXPECT_TRUE(state->allocations.emplace(result, count).second);
        ++state->allocated;
        return result;
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        if (pointer != nullptr)
        {
            const auto it = state->allocations.find(pointer);
            ASSERT_NE(it, state->allocations.end()) << "Wrong allocation owner";
            EXPECT_EQ(it->second, count);
            for (std::size_t i = 0; i < count; ++i)
            {
                EXPECT_EQ(state->objects.count(pointer + i), 0U);
            }
            std::allocator<T>{}.deallocate(pointer, it->second);
            state->allocations.erase(it);
            ++state->deallocated;
        }
    }
    template <typename U, typename... Args>
    void construct(U* pointer, Args&&... args)
    {
        if (state->fail_construct)
        {
            if (state->constructions_before_failure == 0)
            {
                throw Failure{};
            }
            --state->constructions_before_failure;
        }
        ::new (static_cast<void*>(pointer)) U(std::forward<Args>(args)...);
        // VLA deliberately elides trivial construction/destruction in its storage fast paths.
        if (!std::is_trivially_destructible<U>::value)
        {
            EXPECT_TRUE(state->objects.insert(pointer).second);
        }
    }
    template <typename U>
    void destroy(U* pointer)
    {
        if (!std::is_trivially_destructible<U>::value)
        {
            EXPECT_EQ(state->objects.erase(pointer), 1U);
        }
        pointer->~U();
    }
    template <typename U>
    bool operator==(const Allocator<U, Propagate>& rhs) const noexcept
    {
        return state == rhs.state;
    }
    template <typename U>
    bool operator!=(const Allocator<U, Propagate>& rhs) const noexcept
    {
        return !(*this == rhs);
    }
};

template <typename T = Value>
using Array = cetl::VariableLengthArray<T, Allocator<T>>;

template <typename Subject>
void populate(Subject& array, LifetimeState& state, std::size_t count)
{
    array.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        array.emplace_back(state, static_cast<int>(i + 10));
    }
}

template <typename Subject>
void expect_values(const Subject& array, std::size_t count)
{
    ASSERT_EQ(array.size(), count);
    for (std::size_t i = 0; i < count; ++i)
    {
        EXPECT_EQ(array[i].value, static_cast<int>(i + 10));
    }
}

TEST(VLAExceptionSafety, FailedListRangeAndCopyConstructorsReleaseStorage)
{
    for (int kind = 0; kind < 4; ++kind)
    {
        for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
        {
            SCOPED_TRACE(::testing::Message() << "kind=" << kind << " fail_at=" << fail_at);
            LifetimeState   lifetimes;
            AllocationState allocations;
            const auto      list = {Value{lifetimes, 10}, Value{lifetimes, 11}, Value{lifetimes, 12}};
            Array<>         source{list, Allocator<Value>{allocations}};
            const auto      constructed = lifetimes.constructed;
            const auto      destroyed   = lifetimes.destroyed;
            lifetimes.arm(Operation::Copy, fail_at);
            if (kind == 0)
            {
                EXPECT_THROW((Array<>{list, Allocator<Value>{allocations}}), Failure);
            }
            if (kind == 1)
            {
                EXPECT_THROW((Array<>{list.begin(), list.end(), Allocator<Value>{allocations}}), Failure);
            }
            if (kind == 2)
            {
                EXPECT_THROW((Array<>{source}), Failure);
            }
            if (kind == 3)
            {
                EXPECT_THROW((Array<>{source, Allocator<Value>{allocations}}), Failure);
            }
            EXPECT_EQ(lifetimes.constructed - constructed, fail_at);
            EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at);
            EXPECT_EQ(lifetimes.live.size(), 6U);
            EXPECT_EQ(allocations.allocations.size(), 1U);
            EXPECT_EQ(allocations.objects.size(), 3U);
            expect_values(source, 3);
        }
    }
}

template <typename T>
void check_relocation(Operation operation)
{
    for (int kind = 0; kind < 3; ++kind)
    {
        for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
        {
            SCOPED_TRACE(::testing::Message() << "kind=" << kind << " fail_at=" << fail_at);
            LifetimeState   lifetimes;
            AllocationState allocations;
            AllocationState destination;
            Array<T>        source{Allocator<T>{allocations}};
            source.reserve(8);
            populate(source, lifetimes, 3);
            const auto old_data    = source.data();
            const auto constructed = lifetimes.constructed;
            const auto destroyed   = lifetimes.destroyed;
            lifetimes.arm(operation, fail_at);
            if (kind == 0)
            {
                EXPECT_THROW(source.reserve(16), Failure);
            }
            if (kind == 1)
            {
                EXPECT_THROW(source.shrink_to_fit(), Failure);
            }
            if (kind == 2)
            {
                EXPECT_THROW((Array<T>{std::move(source), Allocator<T>{destination}}), Failure);
            }
            EXPECT_EQ(lifetimes.constructed - constructed, fail_at);
            EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at);
            EXPECT_EQ(lifetimes.live.size(), 3U);
            EXPECT_EQ(source.data(), old_data);
            EXPECT_EQ(source.size(), 3U);
            EXPECT_EQ(source.capacity(), 8U);
            EXPECT_EQ(allocations.allocations.size(), 1U);
            EXPECT_EQ(allocations.objects.size(), 3U);
            EXPECT_TRUE(destination.allocations.empty());
            EXPECT_TRUE(destination.objects.empty());
            if (operation == Operation::Copy)
            {
                expect_values(source, 3);
            }
            lifetimes.operation = Operation::None;
            source.clear();
            populate(source, lifetimes, 3);
            source.reserve(16);
            source.shrink_to_fit();
            expect_values(source, 3);
        }
    }
}

TEST(VLAExceptionSafety, CopyRelocationPreservesOriginalValues)
{
    check_relocation<Value>(Operation::Copy);
}
TEST(VLAExceptionSafety, ThrowingMoveOnlyRelocationPreservesOwnership)
{
    check_relocation<MoveOnly>(Operation::Move);
}

TEST(VLAExceptionSafety, ResizeAndAssignRollBackTheNewSuffix)
{
    for (int operation = 0; operation < 3; ++operation)
    {
        for (bool spare : {false, true})
        {
            for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
            {
                SCOPED_TRACE(::testing::Message()
                             << "operation=" << operation << " spare=" << spare << " fail_at=" << fail_at);
                LifetimeState lifetimes;
                default_state = &lifetimes;
                AllocationState allocations;
                Array<>         array{Allocator<Value>{allocations}};
                array.reserve(spare ? 8 : 2);
                populate(array, lifetimes, 2);
                const Value fill{lifetimes, 42};
                lifetimes.arm(operation == 0 ? Operation::Default : Operation::Copy,
                              fail_at + ((!spare && operation != 0) ? 2 : 0));
                const auto constructed = lifetimes.constructed;
                const auto destroyed   = lifetimes.destroyed;
                if (operation == 0)
                {
                    EXPECT_THROW(array.resize(5), Failure);
                }
                if (operation == 1)
                {
                    EXPECT_THROW(array.resize(5, fill), Failure);
                }
                if (operation == 2)
                {
                    EXPECT_THROW(array.assign(5, fill), Failure);
                }
                EXPECT_EQ(lifetimes.constructed - constructed, fail_at + (spare ? 0 : 2));
                EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at + (spare ? 0 : 2));
                EXPECT_EQ(lifetimes.live.size(), 3U);
                expect_values(array, 2);
                EXPECT_EQ(array.capacity(), spare ? 8U : 5U);
                EXPECT_EQ(allocations.objects.size(), 2U);
                EXPECT_EQ(allocations.allocations.size(), 1U);
                lifetimes.operation = Operation::None;
                array.resize(5, fill);
                EXPECT_EQ(array.size(), 5U);
                array.clear();
                EXPECT_TRUE(allocations.objects.empty());
                array.emplace_back(lifetimes, 7);
                EXPECT_EQ(array.back().value, 7);
            }
        }
    }
    default_state = nullptr;
}

template <typename Subject>
void assign_subject(Subject& target, Subject& source, std::true_type)
{
    target = std::move(source);
}
template <typename Subject>
void assign_subject(Subject& target, Subject& source, std::false_type)
{
    target = source;
}

template <typename T, bool Propagate, bool Move>
void check_assignment()
{
    using Subject = cetl::VariableLengthArray<T, Allocator<T, Propagate>>;
    for (bool spare : {false, true})
    {
        for (std::size_t fail_at = 0; fail_at < (spare && !Propagate ? 3U : 4U); ++fail_at)
        {
            SCOPED_TRACE(::testing::Message() << "spare=" << spare << " fail_at=" << fail_at);
            LifetimeState   lifetimes;
            AllocationState source_allocations;
            AllocationState allocations;
            Subject         source{Allocator<T, Propagate>{source_allocations}};
            Subject         target{Allocator<T, Propagate>{allocations}};
            populate(source, lifetimes, 4);
            target.reserve(spare ? 8 : 1);
            populate(target, lifetimes, 1);
            const auto constructed = lifetimes.constructed;
            const auto destroyed   = lifetimes.destroyed;
            lifetimes.arm(Move ? Operation::Move : Operation::Copy, fail_at);
            EXPECT_THROW(assign_subject(target, source, std::integral_constant<bool, Move>{}), Failure);
            const bool retained = spare && !Propagate;
            EXPECT_EQ(target.size(), retained ? 1U : 0U);
            EXPECT_EQ(source.size(), 4U);
            EXPECT_EQ(lifetimes.constructed - constructed, fail_at);
            EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at + (retained ? 0U : 1U));
            EXPECT_EQ(lifetimes.live.size(), retained ? 5U : 4U);
            EXPECT_EQ(source_allocations.allocations.size(), Propagate ? 2U : 1U);
            EXPECT_EQ(allocations.allocations.size(), Propagate ? 0U : 1U);
            if (!Move)
            {
                expect_values(source, 4);
            }
            lifetimes.operation = Operation::None;
            assign_subject(target, source, std::integral_constant<bool, Move>{});
            if (Move)
            {
                EXPECT_TRUE(source.empty());
            }
            else
            {
                expect_values(target, 4);
            }
            EXPECT_EQ(target.size(), 4U);
            target.clear();
            target.emplace_back(lifetimes, 99);
            EXPECT_EQ(target.back().value, 99);
        }
    }
}

TEST(VLAExceptionSafety, CopyAssignmentRollsBackUninitializedElements)
{
    check_assignment<Value, false, false>();
}
TEST(VLAExceptionSafety, PropagatingCopyAssignmentRetainsCorrectAllocator)
{
    check_assignment<Value, true, false>();
}
TEST(VLAExceptionSafety, UnequalAllocatorMoveAssignmentRollsBackUninitializedElements)
{
    check_assignment<MoveOnly, false, true>();
}

TEST(VLAExceptionSafety, AllocatorConstructionFailuresRollBackCompletedObjects)
{
    for (int kind = 0; kind < 3; ++kind)
    {
        for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
        {
            SCOPED_TRACE(::testing::Message() << "kind=" << kind << " fail_at=" << fail_at);
            LifetimeState   lifetimes;
            AllocationState allocations;
            Array<>         array{Allocator<Value>{allocations}};
            array.reserve(8);
            populate(array, lifetimes, 3);
            const auto constructed                   = lifetimes.constructed;
            const auto destroyed                     = lifetimes.destroyed;
            allocations.fail_construct               = true;
            allocations.constructions_before_failure = fail_at;
            if (kind == 0)
            {
                EXPECT_THROW(array.reserve(16), Failure);
            }
            if (kind == 1)
            {
                EXPECT_THROW(array.resize(6, array[0]), Failure);
            }
            if (kind == 2)
            {
                EXPECT_THROW((Array<>{array}), Failure);
            }
            EXPECT_EQ(lifetimes.constructed - constructed, fail_at);
            EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at);
            EXPECT_EQ(lifetimes.live.size(), 3U);
            EXPECT_EQ(allocations.objects.size(), 3U);
            EXPECT_EQ(allocations.allocations.size(), 1U);
            EXPECT_EQ(array.capacity(), 8U);
            expect_values(array, 3);
            allocations.fail_construct = false;
            array.resize(6, array[0]);
            EXPECT_EQ(array.size(), 6U);
            array.clear();
            EXPECT_TRUE(lifetimes.live.empty());
            EXPECT_TRUE(allocations.objects.empty());
        }
    }
}

struct ThrowingRange
{
    Value*         data;
    LifetimeState* state;
    Value&         operator[](std::ptrdiff_t index) const
    {
        state->attempt(Operation::Read);
        return data[index];
    }
    bool operator>=(const ThrowingRange& rhs) const
    {
        return data >= rhs.data;
    }
    std::ptrdiff_t operator-(const ThrowingRange& rhs) const
    {
        return data - rhs.data;
    }
};

TEST(VLAExceptionSafety, RangeAccessFailuresRollBackCompletedObjects)
{
    for (std::size_t fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        LifetimeState       lifetimes;
        AllocationState     allocations;
        Value               values[] = {{lifetimes, 10}, {lifetimes, 11}, {lifetimes, 12}};
        const ThrowingRange first{values, &lifetimes};
        const ThrowingRange last{values + 3, &lifetimes};
        const auto          constructed = lifetimes.constructed;
        const auto          destroyed   = lifetimes.destroyed;
        lifetimes.arm(Operation::Read, fail_at);
        EXPECT_THROW((Array<>{first, last, Allocator<Value>{allocations}}), Failure);
        EXPECT_EQ(lifetimes.constructed - constructed, fail_at);
        EXPECT_EQ(lifetimes.destroyed - destroyed, fail_at);
        EXPECT_EQ(lifetimes.live.size(), 3U);
        EXPECT_TRUE(allocations.objects.empty());
        EXPECT_TRUE(allocations.allocations.empty());
        lifetimes.operation = Operation::None;
        Array<> array{first, last, Allocator<Value>{allocations}};
        expect_values(array, 3);
    }
}

TEST(VLAExceptionSafety, EmptyRangesDoNotInvokeConstruction)
{
    LifetimeState   lifetimes;
    AllocationState allocations;
    allocations.fail_construct = true;
    const std::initializer_list<Value> empty;
    Array<>                            source{empty, Allocator<Value>{allocations}};
    Array<>                            copy{source};
    // Reserving an empty nontrivial array exercises a zero-length relocation into allocated storage.
    source.reserve(4);
    EXPECT_EQ(source.capacity(), 4U);
    EXPECT_EQ(lifetimes.constructed, 0U);
    EXPECT_TRUE(allocations.objects.empty());
    EXPECT_TRUE(copy.empty());
    EXPECT_TRUE(source.empty());
    allocations.fail_construct = false;
    populate(source, lifetimes, 3);
    expect_values(source, 3);
}

TEST(VLAExceptionSafety, AssignmentExceptionsLeaveExistingObjectsAlive)
{
    for (int kind = 0; kind < 3; ++kind)
    {
        LifetimeState   lifetimes;
        AllocationState allocations;
        Array<>         source{Allocator<Value>{allocations}};
        Array<>         target{Allocator<Value>{allocations}};
        populate(source, lifetimes, 3);
        populate(target, lifetimes, 3);
        lifetimes.arm(Operation::Assign, 1);
        if (kind == 0)
        {
            EXPECT_THROW(target = source, Failure);
        }
        if (kind == 1)
        {
            EXPECT_THROW(target.assign(3, source[0]), Failure);
        }
        if (kind == 2)
        {
            AllocationState other;
            Array<MoveOnly> movable{Allocator<MoveOnly>{other}};
            Array<MoveOnly> destination{Allocator<MoveOnly>{allocations}};
            populate(movable, lifetimes, 3);
            populate(destination, lifetimes, 3);
            EXPECT_THROW(destination = std::move(movable), Failure);
            EXPECT_EQ(destination.size(), 3U);
            EXPECT_EQ(movable.size(), 3U);
        }
        EXPECT_EQ(target.size(), 3U);
        EXPECT_EQ(lifetimes.live.size(), 6U);
        EXPECT_EQ(allocations.objects.size(), 6U);
        lifetimes.operation = Operation::None;
        target.clear();
        target = source;
        expect_values(target, 3);
    }
}

struct ThrowingBool
{
    LifetimeState* state;
    operator bool() const
    {
        state->attempt(Operation::Copy);
        return true;
    }
};

TEST(VLAExceptionSafety, BoolRangeFailureReleasesStorage)
{
    for (std::size_t fail_at : {0U, 4U, 8U})
    {
        LifetimeState   lifetimes;
        AllocationState allocations;
        ThrowingBool    values[9];
        for (auto& value : values)
        {
            value.state = &lifetimes;
        }
        lifetimes.arm(Operation::Copy, fail_at);
        using Subject = cetl::VariableLengthArray<bool, Allocator<bool>>;
        EXPECT_THROW((Subject{std::begin(values), std::end(values), Allocator<bool>{allocations}}), Failure);
        EXPECT_TRUE(allocations.allocations.empty());
    }
}

TEST(VLAExceptionSafety, BoolAssignmentAllocationFailureLeavesReusableEmptyContainer)
{
    using Subject = cetl::VariableLengthArray<bool, Allocator<bool>>;
    for (bool move : {false, true})
    {
        AllocationState source_allocations;
        AllocationState allocations;
        Subject         source{Allocator<bool>{source_allocations}};
        Subject         target{Allocator<bool>{allocations}};
        source.resize(17, true);
        target.resize(3, false);
        allocations.fail = true;
        if (move)
        {
            EXPECT_THROW(target = std::move(source), std::bad_alloc);
        }
        else
        {
            EXPECT_THROW(target = source, std::bad_alloc);
        }
        EXPECT_EQ(target.size(), 0U);
        EXPECT_EQ(target.capacity(), 0U);
        EXPECT_TRUE(allocations.allocations.empty());
        EXPECT_EQ(source.size(), 17U);
        allocations.fail = false;
        target.emplace_back(true);
        EXPECT_EQ(target.size(), 1U);
        EXPECT_TRUE(target[0]);
        if (move)
        {
            target = std::move(source);
            EXPECT_TRUE(source.empty());
            source.emplace_back(false);
            EXPECT_EQ(source.size(), 1U);
            EXPECT_FALSE(source[0]);
        }
        else
        {
            target = source;
        }
        EXPECT_EQ(target.size(), 17U);
        for (bool value : target)
        {
            EXPECT_TRUE(value);
        }
    }
}

TEST(VLAExceptionSafety, AllocationFailurePreservesExistingStorage)
{
    LifetimeState   lifetimes;
    AllocationState allocations;
    Array<>         source{Allocator<Value>{allocations}};
    source.reserve(8);
    populate(source, lifetimes, 3);
    auto* const original = source.data();
    allocations.fail     = true;
    EXPECT_THROW(source.reserve(16), std::bad_alloc);
    EXPECT_NO_THROW(source.shrink_to_fit());
    EXPECT_THROW(source.resize(16), std::bad_alloc);
    EXPECT_EQ(source.data(), original);
    EXPECT_EQ(source.capacity(), 8U);
    expect_values(source, 3);
    EXPECT_EQ(lifetimes.live.size(), 3U);
    EXPECT_EQ(allocations.objects.size(), 3U);
    EXPECT_EQ(allocations.allocations.size(), 1U);
    allocations.fail = false;
    source.emplace_back(lifetimes, 13);
    expect_values(source, 4);
}

#else
TEST(VLAExceptionSafety, ExceptionsDisabled)
{
    GTEST_SKIP() << "Exception injection requires exceptions enabled.";
}
#endif
}  // namespace
