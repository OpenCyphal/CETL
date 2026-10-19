/// @file
/// Stateful differential fuzzing of VariableLengthArray against std::vector.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <forward_list>
#include <iterator>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
constexpr std::size_t MaxElements   = 256;
constexpr std::size_t MaxOperations = 256;

// Each instruction has three bytes: operation/array selector, count/index, and value.
constexpr std::size_t InstructionSize = 3;

// Bits of the header byte that choose the allocator configuration.
constexpr unsigned UseStdAllocatorBit     = 4U;
constexpr unsigned ShareAllocatorStateBit = 8U;
constexpr unsigned ArraySelectorShift     = 4U;
constexpr unsigned AssignVariantShift     = 5U;
constexpr unsigned OperationCount         = 16U;
constexpr unsigned StringLengthModulus    = 64U;
constexpr unsigned StringAlphabetSize     = 26U;

enum class Operation : std::uint8_t
{
    PushBackCopy,
    PushBackMove,
    EmplaceBackOrInsert,
    PopBackOrErase,
    Resize,
    ResizeValue,
    Assign,
    Reserve,
    ShrinkToFit,
    Clear,
    IndexAssign,
    CopyAssign,
    MoveAssign,
    CopyConstruct,
    MoveConstruct,
    SelfAssign,
};
static_assert(static_cast<unsigned>(Operation::SelfAssign) + 1U == OperationCount, "operation count mismatch");

// Unlike assert(), these checks remain active in Release builds.
void require(const bool condition, const char* const message)
{
    if (!condition)
    {
        (void) std::fputs("VLA fuzz invariant failed: ", stderr);
        (void) std::fputs(message, stderr);
        (void) std::fputs("\n", stderr);
        std::abort();
    }
}

/// Ledger of live allocations shared by every TrackingAllocator bound to it.
///
/// Records each allocation's address and byte size, and aborts on a duplicate allocation, a deallocation of an
/// unknown pointer (wrong allocator or double free), a size mismatch, or any allocation still live at destruction.
/// Allocators hold a pointer to their state, so it is neither copyable nor movable and must outlive every container
/// using it. Allocator equality is the identity of this state.
class AllocationState
{
public:
    AllocationState()                                  = default;
    AllocationState(const AllocationState&)            = delete;
    AllocationState(AllocationState&&)                 = delete;
    AllocationState& operator=(const AllocationState&) = delete;
    AllocationState& operator=(AllocationState&&)      = delete;

    ~AllocationState()
    {
        require(allocations_.empty(), "leaked container allocation");
    }

    void add(void* const pointer, const std::size_t bytes)
    {
        require(allocations_.emplace(pointer, bytes).second, "duplicate allocation");
    }

    void remove(void* const pointer, const std::size_t bytes)
    {
        const auto allocation = allocations_.find(pointer);
        require(allocation != allocations_.end(), "deallocation by wrong allocator or double free");
        require(allocation->second == bytes, "incorrect deallocation size");
        allocations_.erase(allocation);
    }

private:
    std::map<void*, std::size_t> allocations_;
};

// Two state identities exercise equal and unequal, non-propagating allocators, including bool's rebind.
template <typename T>
class TrackingAllocator
{
public:
    using value_type                             = T;
    using is_always_equal                        = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;

    explicit TrackingAllocator(AllocationState& state) noexcept
        : state_(&state)
    {
    }

    // Allocator requirements call for an implicit rebinding conversion.
    template <typename U>
    // NOLINTNEXTLINE(*-explicit-constructor,*-explicit-conversions)
    TrackingAllocator(const TrackingAllocator<U>& other) noexcept
        : state_(other.state_)
    {
    }

    T* allocate(const std::size_t count)
    {
        auto* const pointer = std::allocator<T>{}.allocate(count);
        state_->add(pointer, count * sizeof(T));
        return pointer;
    }

    void deallocate(T* const pointer, const std::size_t count) noexcept
    {
        if (pointer == nullptr)
        {
            require(count == 0, "null allocation has nonzero capacity");
            return;
        }
        state_->remove(pointer, count * sizeof(T));
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template <typename U>
    bool operator==(const TrackingAllocator<U>& other) const noexcept
    {
        return state_ == other.state_;
    }

    template <typename U>
    bool operator!=(const TrackingAllocator<U>& other) const noexcept
    {
        return !(*this == other);
    }

private:
    template <typename U>
    friend class TrackingAllocator;

    AllocationState* state_;
};

template <typename T>
struct ValueFactory
{
    static T make(const std::uint8_t byte)
    {
        return static_cast<T>(byte);
    }
};

template <>
struct ValueFactory<std::string>
{
    static std::string make(const std::uint8_t byte)
    {
        // Include both short strings and heap-owning strings to exercise non-trivial relocation/destruction.
        // Avoid braced initialization, which would select the initializer_list<char> constructor.
        std::string value(byte % StringLengthModulus, static_cast<char>('a' + (byte % StringAlphabetSize)));
        return value;
    }
};

// The libFuzzer runtime is not built with ASan, but some distributions (Homebrew LLVM on macOS) export its
// std::vector<std::string> instantiations. Under -fno-exceptions those share libc++'s ABI tag with the harness's, so
// the linker may keep the uninstrumented copies and leave the model's container annotations stale, which ASan then
// reports as a container-overflow. A distinct allocator type gives the models instantiations of their own.
template <typename T>
class ModelAllocator : public std::allocator<T>
{
public:
    // std::allocator<T>::rebind would otherwise be inherited before C++20.
    template <typename U>
    struct rebind
    {
        using other = ModelAllocator<U>;
    };

    ModelAllocator() noexcept = default;

    template <typename U>
    // NOLINTNEXTLINE(*-explicit-constructor,*-explicit-conversions)
    ModelAllocator(const ModelAllocator<U>&) noexcept
    {
    }
};

template <typename T>
using Model = std::vector<T, ModelAllocator<T>>;

// Single-pass view of a model, for the input-iterator assign path.
template <typename T>
class SinglePass
{
public:
    using iterator_category = std::input_iterator_tag;
    using value_type        = T;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const T*;
    using reference         = T;

    SinglePass(const Model<T>& source, const std::size_t index) noexcept
        : source_(&source)
        , index_(index)
    {
    }
    T operator*() const
    {
        return source_->at(index_);
    }
    SinglePass& operator++() noexcept
    {
        ++index_;
        return *this;
    }
    SinglePass operator++(int) noexcept
    {
        const auto old = *this;
        ++index_;
        return old;
    }
    bool operator==(const SinglePass& rhs) const noexcept
    {
        return index_ == rhs.index_;
    }
    bool operator!=(const SinglePass& rhs) const noexcept
    {
        return !(*this == rhs);
    }

private:
    const Model<T>* source_;
    std::size_t     index_;
};

// Assigns one of the range overloads to both the array and its model. The opcode's spare bits pick the overload.
template <typename T, typename Array, typename Other>
void assign_variant(const unsigned    variant,
                    Array&            array,
                    Model<T>&         model,
                    Other&            other,
                    const Model<T>&   other_model,
                    const std::size_t count,
                    const T&          value)
{
    // A prefix of the other model keeps sources within max_size().
    const auto prefix =
        Model<T>(other_model.begin(),
                 other_model.begin() + static_cast<std::ptrdiff_t>(std::min(count, other_model.size())));
    switch (variant)
    {
    case 1:  // random access
        array.assign(prefix.begin(), prefix.end());
        model.assign(prefix.begin(), prefix.end());
        break;
    case 2:  // forward only
    {
        const std::forward_list<T> list(prefix.begin(), prefix.end());
        array.assign(list.begin(), list.end());
        model.assign(prefix.begin(), prefix.end());
        break;
    }
    case 3:  // single pass
        array.assign(SinglePass<T>(prefix, 0), SinglePass<T>(prefix, prefix.size()));
        model.assign(prefix.begin(), prefix.end());
        break;
    case 4:  // initializer list
        array.assign({value, T{}, value});
        model.assign({value, T{}, value});
        break;
    case 5:  // another array, including packed-bool proxy iterators
        array.assign(other.begin(), other.end());
        model.assign(other_model.begin(), other_model.end());
        break;
    case 6:  // longer than max_size(): rejected before any change, or truncated without exceptions
    {
        const Model<T> oversized(MaxElements + 1U + count, value);
#if defined(__cpp_exceptions)
        bool rejected = false;
        try
        {
            array.assign(oversized.begin(), oversized.end());
        } catch (const std::length_error&)
        {
            rejected = true;
        }
        require(rejected, "oversized assign was accepted");
#else
        array.assign(oversized.begin(), oversized.end());
        model.assign(oversized.begin(), oversized.begin() + static_cast<std::ptrdiff_t>(MaxElements));
#endif
        break;
    }
    default:
        array.assign(count, value);
        model.assign(count, value);
        break;
    }
}

// Inserts [first, last) into the model at `at` by rebuilding it. libc++ 23's std::vector<bool>::insert shifts a word by
// its full width in __copy_backward_unaligned, which UBSan reports, so the model does not call insert().
template <typename T, typename It>
void model_insert(Model<T>& model, const std::size_t at, It first, It last)
{
    Model<T> updated;
    updated.reserve(model.size() + static_cast<std::size_t>(std::distance(first, last)));
    for (std::size_t i = 0; i < at; ++i)
    {
        updated.push_back(model.at(i));
    }
    for (; first != last; ++first)
    {
        updated.push_back(*first);
    }
    for (std::size_t i = at; i < model.size(); ++i)
    {
        updated.push_back(model.at(i));
    }
    model.swap(updated);
}

// Appends or inserts into both the array and its model. The opcode's spare bits pick the operation, and every insertion
// is bounded so the array stays within max_size(). Each insert must return the position of its first new element.
template <typename T, typename Array>
void emplace_or_insert(const unsigned    variant,
                       Array&            array,
                       Model<T>&         model,
                       const Model<T>&   other_model,
                       const std::size_t count,
                       const T&          value)
{
    const std::size_t room = MaxElements - model.size();
    if (room == 0U)
    {
        return;
    }
    // Inserting before end() is included.
    const std::size_t at       = count % (model.size() + 1U);
    const auto        offset   = static_cast<std::ptrdiff_t>(at);
    const std::size_t inserted = std::min(room, count % 8U);
    const auto        take     = static_cast<std::ptrdiff_t>(std::min(inserted, other_model.size()));
    std::ptrdiff_t    result   = offset;
    switch (variant)
    {
    case 1:  // one copy
    {
        const auto position = array.insert(array.cbegin() + offset, value);
        result              = position - array.begin();
        const Model<T> one(1U, value);
        model_insert(model, at, one.begin(), one.end());
        break;
    }
    case 2:  // several copies, including none
    {
        const auto position = array.insert(array.cbegin() + offset, inserted, value);
        result              = position - array.begin();
        const Model<T> copies(inserted, value);
        model_insert(model, at, copies.begin(), copies.end());
        break;
    }
    case 3:  // forward range from the other model
    {
        const std::forward_list<T> list(other_model.begin(), other_model.begin() + take);
        const auto                 position = array.insert(array.cbegin() + offset, list.begin(), list.end());
        result                              = position - array.begin();
        model_insert(model, at, other_model.begin(), other_model.begin() + take);
        break;
    }
    case 4:  // single-pass range from the other model
    {
        const auto position = array.insert(array.cbegin() + offset,
                                           SinglePass<T>(other_model, 0),
                                           SinglePass<T>(other_model, static_cast<std::size_t>(take)));
        result              = position - array.begin();
        model_insert(model, at, other_model.begin(), other_model.begin() + take);
        break;
    }
    case 5:  // initializer list
        if (room >= 2U)
        {
            const auto position = array.insert(array.cbegin() + offset, {value, T{}});
            result              = position - array.begin();
            const Model<T> list{value, T{}};
            model_insert(model, at, list.begin(), list.end());
        }
        break;
    default:
        array.emplace_back(value);
        model.push_back(value);
        return;
    }
    require(result == offset, "insert returned the wrong position");
}

template <typename T, typename Allocator>
void check(cetl::VariableLengthArray<T, Allocator>& actual, const Model<T>& expected)
{
    require(actual.size() == expected.size(), "size differs from std::vector");
    require(actual.empty() == expected.empty(), "empty differs from std::vector");
    require(actual.size() <= actual.capacity(), "size exceeds capacity");
    require(actual.capacity() <= actual.max_size(), "capacity exceeds max_size");
    require(actual.max_size() == MaxElements, "max_size changed");
    const auto& const_actual = actual;
    for (std::size_t i = 0; i < expected.size(); ++i)
    {
        // operator[] is the API under test; the loop bound was checked against size() above.
        // NOLINTNEXTLINE(*-unchecked-container-access)
        require(actual[i] == expected.at(i) && const_actual[i] == expected.at(i), "indexed value differs");
    }
    require(std::distance(actual.begin(), actual.end()) == static_cast<std::ptrdiff_t>(expected.size()) &&
                std::distance(const_actual.cbegin(), const_actual.cend()) ==
                    static_cast<std::ptrdiff_t>(expected.size()),
            "iterator range length differs");
    require(std::equal(actual.begin(), actual.end(), expected.begin(), expected.end()), "iterator value differs");
    require(std::equal(const_actual.cbegin(), const_actual.cend(), expected.begin(), expected.end()),
            "const iterator value differs");
    if (!expected.empty())
    {
        require(actual.front() == expected.front() && actual.back() == expected.back(), "endpoint value differs");
        require(const_actual.front() == expected.front() && const_actual.back() == expected.back(),
                "const endpoint value differs");
    }
}

template <typename T, typename Allocator>
void exercise(const std::vector<std::uint8_t>& input, const Allocator& first, const Allocator& second)
{
    using Array = cetl::VariableLengthArray<T, Allocator>;
    std::array<Array, 2>    arrays{{Array(MaxElements, first), Array(MaxElements, second)}};
    std::array<Model<T>, 2> models{};

    // Skip the header byte. Ignore an incomplete trailing instruction, and bound work even when replaying an
    // oversized artifact.
    for (std::size_t offset = 1, step = 0; (offset + InstructionSize) <= input.size() && step < MaxOperations;
         offset += InstructionSize, ++step)
    {
        const unsigned    opcode      = input.at(offset);
        const unsigned    count_byte  = input.at(offset + 1);
        const auto        value_byte  = input.at(offset + 2);
        const std::size_t selected    = (opcode >> ArraySelectorShift) & 1U;
        auto&             array       = arrays.at(selected);
        auto&             model       = models.at(selected);
        auto&             other       = arrays.at(1U - selected);
        auto&             other_model = models.at(1U - selected);
        const std::size_t count       = count_byte + (value_byte & 1U);
        const auto        value       = ValueFactory<T>::make(value_byte);
        const auto        capacity    = array.capacity();

        switch (static_cast<Operation>(opcode % OperationCount))
        {
        case Operation::PushBackCopy:
            if (model.size() < MaxElements)
            {
                array.push_back(value);
                model.push_back(value);
            }
            break;
        case Operation::PushBackMove:
            if (model.size() < MaxElements)
            {
                auto movable = value;
                array.push_back(std::move(movable));
                model.push_back(value);
            }
            break;
        case Operation::EmplaceBackOrInsert:
            emplace_or_insert<T>(opcode >> AssignVariantShift, array, model, other_model, count, value);
            break;
        case Operation::PopBackOrErase: {
            // The opcode's spare bits choose pop_back, erase(pos), or erase(first, last).
            const unsigned variant = opcode >> AssignVariantShift;
            const auto     size    = model.size();
            if (variant == 0U)
            {
                array.pop_back();  // CETL explicitly permits popping an empty array.
                if (!model.empty())
                {
                    model.pop_back();
                }
            }
            else if (((variant & 1U) != 0U) && (size > 0U))
            {
                const auto at     = static_cast<std::ptrdiff_t>(count % size);
                const auto result = array.erase(array.cbegin() + at);
                (void) model.erase(model.begin() + at);
                require((result - array.begin()) == at, "erase(pos) returned the wrong position");
            }
            else if (size == 0U)
            {
                // The only range of an empty array; it may have no storage, so avoid arithmetic on null pointers.
                const auto result = array.erase(array.cbegin(), array.cend());
                require(result == array.end(), "erasing an empty array did not return end()");
            }
            else
            {
                // Includes empty ranges, at any position including end().
                const std::size_t first  = count % (size + 1U);
                const std::size_t last   = first + (value_byte % (size + 1U - first));
                const auto        from   = static_cast<std::ptrdiff_t>(first);
                const auto        to     = static_cast<std::ptrdiff_t>(last);
                const auto        result = array.erase(array.cbegin() + from, array.cbegin() + to);
                (void) model.erase(model.begin() + from, model.begin() + to);
                require((result - array.begin()) == from, "erase(first, last) returned the wrong position");
            }
            require(array.capacity() == capacity, "pop_back or erase changed capacity");
            break;
        }
        case Operation::Resize:
            array.resize(count);
            model.resize(count);
            break;
        case Operation::ResizeValue:
            array.resize(count, value);
            model.resize(count, value);
            break;
        case Operation::Assign:
            assign_variant<T>(opcode >> AssignVariantShift, array, model, other, other_model, count, value);
            break;
        case Operation::Reserve:
            array.reserve(count);
            require(array.capacity() >= std::max(capacity, count), "reserve lost capacity");
            break;
        case Operation::ShrinkToFit:
            array.shrink_to_fit();
            require(array.capacity() <= capacity, "shrink_to_fit grew capacity");
            break;
        case Operation::Clear:
            array.clear();
            model.clear();
            require(array.capacity() == capacity, "clear changed capacity");
            break;
        case Operation::IndexAssign:
            if (!model.empty())
            {
                const auto index = count % model.size();
                array[index]     = value;  // NOLINT(*-unchecked-container-access) operator[] is under test.
                model.at(index)  = value;
            }
            break;
        case Operation::CopyAssign:
            array = other;
            model = other_model;
            break;
        case Operation::MoveAssign:
            array = std::move(other);
            model = other_model;
            other_model.clear();  // CETL guarantees an empty source; std::vector's moved state is unspecified.
            break;
        case Operation::CopyConstruct: {
            Array copy = ((count & 1U) != 0U) ? Array(other, array.get_allocator()) : Array(other);
            check(copy, other_model);
            // Use vector iterators so an empty range never subtracts null raw pointers in the harness.
            Array range(other_model.begin(), other_model.end(), MaxElements, array.get_allocator());
            check(range, other_model);
            // count never exceeds MaxElements.
            Array fill(count, value, MaxElements, array.get_allocator());
            check(fill, Model<T>(count, value));
            array = copy;
            model = other_model;
            break;
        }
        case Operation::MoveConstruct: {
            Array moved =
                ((count & 1U) != 0U) ? Array(std::move(other), array.get_allocator()) : Array(std::move(other));
            check(moved, other_model);
            array = std::move(moved);
            model = other_model;
            other_model.clear();
            // CETL guarantees an empty source after a move.
            // NOLINTNEXTLINE(*-use-after-move,*-invalid-access-moved)
            require(moved.empty(), "move assignment did not empty temporary");
            break;
        }
        case Operation::SelfAssign: {
            // An alias avoids compiler diagnostics for the intentional self-assignment.
            auto* const self = &array;
            if ((count & 1U) != 0U)
            {
                array = std::move(*self);
            }
            else
            {
                array = *self;
            }
            require(array.capacity() == capacity, "self-assignment changed capacity");
            break;
        }
        }

        check(arrays.front(), models.front());
        check(arrays.back(), models.back());
        require((arrays.front() == arrays.back()) == (models.front() == models.back()),
                "equality differs from std::vector");
        require((arrays.front() != arrays.back()) == (models.front() != models.back()),
                "inequality differs from std::vector");
    }
}

template <typename T>
void run(const std::vector<std::uint8_t>& input)
{
    const unsigned header = input.front();
    if ((header & UseStdAllocatorBit) != 0U)
    {
        // std::allocator also covers the propagating/always-equal move-assignment path.
        exercise<T>(input, std::allocator<T>{}, std::allocator<T>{});
    }
    else
    {
        AllocationState first;
        AllocationState second;
        exercise<T>(input,
                    TrackingAllocator<T>(first),
                    TrackingAllocator<T>(((header & ShareAllocatorStateBit) != 0U) ? first : second));
    }
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size);
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* const data, const std::size_t size)
{
    if (size != 0)
    {
        // libFuzzer's ABI hands over a raw buffer; copy it once so the harness can use checked access.
        const std::vector<std::uint8_t> input(data, data + size);  // NOLINT(*-pointer-arithmetic)
        switch (input.front() % 3U)
        {
        case 0:
            run<int>(input);
            break;
        case 1:
            run<std::string>(input);
            break;
        default:
            run<bool>(input);
            break;
        }
    }
    return 0;
}
