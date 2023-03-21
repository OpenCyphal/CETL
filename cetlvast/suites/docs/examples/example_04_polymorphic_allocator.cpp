/// @file
/// Example of using the CETL realloc_resource type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf17/memory_resource.hpp"
#include <functional>
#include <iostream>
#include <iomanip>
#include <type_traits>
#include <cmath>      // for pow and log
#include <cstring>    // for memcpy
#include <algorithm>  // for min
#include <limits>
#include <iterator>

namespace detail  // similar to the standard library, CETL::detail is stuff that has to be available but shouldn't be
                  // used directly.
{
template <class AlwaysVoid, template <class...> class Op, class... Args>
struct detector
{
    using value_t = std::false_type;
    using type    = void;
};

template <template <class...> class Op, class... Args>
struct detector<std::void_t<Op<Args...>>, Op, Args...>
{
    using value_t = std::true_type;
    using type    = Op<Args...>;
};

}  // namespace detail

template <template <class...> class Op, class... Args>
using is_detected = typename detail::detector<void, Op, Args...>::value_t;

namespace cetl
{
namespace pf17
{
namespace pmr
{

class MallocResource : public cetl::pf17::pmr::memory_resource
{
public:
    std::size_t max_size() const
    {
        return std::numeric_limits<std::size_t>::max();
    }

protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        if (alignment > sizeof(std::max_align_t))
        {
            // C++14 doesn't provide a portable way to over-align.
            // To implement this you'll need to look at detecting POSIX memalign or
            // other platform-specific ways of obtaining over-aligned memory.
            throw std::bad_alloc();
        }
        return std::malloc(size_bytes);
    }
    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        (void) size_bytes;
        (void) alignment;
        std::free(p);
    }
    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (this == &rhs);
    }
};
}  // namespace pmr

template <typename Value,
          typename Comparator = std::greater<Value>,
          typename Allocator  = pmr::polymorphic_allocator<Value>>
class Heap
{
public:
    using allocator_type                           = Allocator;
    using value_type                               = Value;
    using depth_t                                  = std::size_t;
    static constexpr depth_t InitalDepthAllocation = 1;

    class heap_iterator_type
    {
    public:
        // TODO: hold a reference to a value that, if it changes, indicates the iterator is now invalid.
        heap_iterator_type(std::size_t index, Value* heap, std::size_t heap_length, allocator_type alloc)
            : index_(index)
            , heap_(heap)
            , heap_length_(heap_length)
            , alloc_(alloc)
            , children_(nullptr, nullptr)
            , children_are_cached_(false)
        {
        }

        ~heap_iterator_type()                                    = default;
        heap_iterator_type(const heap_iterator_type&)            = default;
        heap_iterator_type& operator=(const heap_iterator_type&) = default;

        Value& operator*()
        {
            return heap_[index_];
        }

        const Value& operator*() const
        {
            return heap_[index_];
        }

        bool operator==(const heap_iterator_type& rhs)
        {
            return (&heap_[index_] == &rhs.heap_[rhs.heap_length_]);
        }

        bool operator!=(const heap_iterator_type& rhs)
        {
            return !operator==(rhs);
        }

        std::size_t id() const
        {
            return index_;
        }

        heap_iterator_type& operator--()
        {
            // decrease the depth
            if (index_ == 0)
            {
                set_index(heap_length_);
            }
            else
            {
                set_index(parent_index_for_index(index_));
            }
            return *this;
        }

        // TODO: Optional<Value>
        const std::pair<const Value*, const Value*>* operator->() const noexcept
        {
            if (!children_are_cached_)
            {
                const std::size_t child1_index = Heap::first_child_of(index_);
                if (child1_index < heap_length_)
                {
                    children_.first = &heap_[child1_index];
                }
                else
                {
                    children_.first = nullptr;
                }
                const std::size_t child2_index = Heap::second_child_of(index_);
                if (child2_index < heap_length_)
                {
                    children_.second = &heap_[child2_index];
                }
                else
                {
                    children_.second = nullptr;
                }
                children_are_cached_ = true;
            }
            return &children_;
        }

        heap_iterator_type& left()
        {
            set_index(std::min(Heap::first_child_of(index_), heap_length_));
            return *this;
        }

        heap_iterator_type& right()
        {
            set_index(std::min(Heap::second_child_of(index_), heap_length_));
            return *this;
        }

    private:
        constexpr void set_index(const std::size_t new_value) noexcept
        {
            if (index_ != new_value)
            {
                children_are_cached_ = false;
                index_               = new_value;
            }
        }

        std::size_t                                   index_;
        Value*                                        heap_;
        std::size_t                                   heap_length_;
        allocator_type                                alloc_;
        mutable std::pair<const Value*, const Value*> children_;
        mutable bool                                  children_are_cached_;
    };

    Heap(const Allocator& alloc) noexcept
        : alloc_(alloc)
        , heap_length_(0)
        , heap_capacity_(0)
        , heap_deleter_([&](Value* heap) { alloc_.deallocate(heap, heap_length_); })
        , heap_(nullptr, heap_deleter_)
    {
    }

    bool reserve(depth_t heap_depth)
    {
        const std::size_t new_capacity = array_capacity_for_depth(heap_depth);
        std::cout << "requesting " << std::setbase(16) << sizeof(Value) * new_capacity << std::setbase(10)
                  << " bytes of capacity (" << new_capacity << " nodes)" << std::endl;
        if (new_capacity < heap_capacity_)
        {
            return true;
        }
        else
        {
            return resize_heap<Allocator>(new_capacity);
        }
    }

    std::size_t capacity() const noexcept
    {
        return heap_capacity_;
    }

    std::size_t length() const noexcept
    {
        return heap_length_;
    }

    bool insert(const Value& v)
    {
        if (!heap_)
        {
            // lazy initialization to avoid throwing bad_alloc from
            // constructor.
            const std::size_t initial_capacity = array_capacity_for_depth(InitalDepthAllocation);
            heap_.reset(alloc_.allocate(initial_capacity));
            if (!heap_)
            {
                return false;
            }
            heap_capacity_ = initial_capacity;
        }

        // [upheap]
        // depth_for_index = log(x+1)/log(2) or 1.4427*log(x+1)
        // width_at_depth = pow(2, depth) or (1 << depth);
        // first_index_for_depth = width_at_depth - 1
        // last_index_for_depth = (1 << (depth + 1)) - 1 or first_index_for_depth + (width_at_depth - 1);
        // length_of_storage_for_depth = sizeof(Value) * ((1 << depth) - 1);
        // parent_index_of_index = floor((index - 1) / 2)
        // parent_count = width_at_depth / 2;
        //
        // [downheap]
        // child0_for_index = 2 * index + 1
        // child1_for_index = 2 * index + 2

        // 1. find the depth of the next open slot.
        // 2. if complete start new level.
        // 3. if not complete insert at next open slot.
        // 4. rotate upheap until max property is restored.

        const std::size_t max_depth = depth_for_index(heap_length_);

        if (heap_capacity_ <= heap_length_)
        {
            // we know we're adding 1 so we'll go ahead and make sure we
            // have enough memory for one more. Because we always allocated
            // a full level at a time this means that adding one more would
            // move us down a level for most cases, except for the initial
            // case!. We start with a heap capacity of 3 and this inital size
            if (!resize_heap<Allocator>(array_capacity_for_depth(max_depth + 1)))
            {
                return false;
            }
        }

        alloc_.construct(&(heap_.get()[heap_length_]), v);
        heap_length_ += 1;

        if (heap_length_ > 1)
        {
            Comparator  comp;
            Value*      heap         = heap_.get();
            std::size_t child_index  = heap_length_ - 1;
            std::size_t parent_index = parent_index_for_index(child_index);
            Value*      child        = &heap[child_index];
            Value*      parent       = &heap[parent_index];
            while (comp(*child, *parent))
            {
                swap(child, parent);
                if (parent_index == 0)
                {
                    break;
                }
                child_index  = parent_index;
                parent_index = parent_index_for_index(child_index);
                child        = &heap[child_index];
                parent       = &heap[parent_index];
            }
        }
        return true;
    }

    Value* begin()
    {
        // TODO: lazy initialize or use a different iterator type.
        return heap_.get();
    }

    Value* end()
    {
        return &heap_.get()[heap_length_];
    }

    heap_iterator_type root()
    {
        return heap_iterator_type(0, heap_.get(), heap_length_, alloc_);
    }

    heap_iterator_type tree_end()
    {
        return heap_iterator_type(heap_length_, heap_.get(), heap_length_, alloc_);
    }

    heap_iterator_type rightmost_leaf()
    {
        if (heap_length_ == 0)
        {
            return tree_end();
        }
        else
        {
            return heap_iterator_type(heap_length_ - 1, heap_.get(), heap_length_, alloc_);
        }
    }

private:
    friend class heap_iterator_type;

    static constexpr float DepthForIndexC = 1.4427f;

    Allocator                   alloc_;
    std::size_t                 heap_length_;
    std::size_t                 heap_capacity_;
    std::function<void(Value*)> heap_deleter_;
    using unique_ptr_t = std::unique_ptr<Value, decltype(heap_deleter_)>;
    unique_ptr_t heap_;

    static constexpr depth_t depth_for_index(std::size_t index)
    {
        // TODO: memoize? Probably a table for some set of depths is reasonable.
        return static_cast<depth_t>(DepthForIndexC * std::log(static_cast<float>(index + 1)));
    }

    static constexpr std::size_t width_at_depth(depth_t depth)
    {
        return (1 << depth);
    }

    static constexpr std::size_t first_index_for_depth(depth_t depth)
    {
        return width_at_depth(depth) - 1;
    }

    static constexpr std::size_t last_index_for_depth(depth_t depth)
    {
        return first_index_for_depth(depth) + (width_at_depth(depth) - 1);
    }

    static constexpr std::size_t array_capacity_for_depth(depth_t depth)
    {
        return last_index_for_depth(depth) + 1;
    }

    static constexpr std::size_t parent_index_for_index(std::size_t index)
    {
        return static_cast<std::size_t>(std::floor(static_cast<float>(index - 1) / 2.0f));
    }

    static constexpr std::size_t first_child_of(std::size_t index)
    {
        return 2 * index + 1;
    }

    static constexpr std::size_t second_child_of(std::size_t index)
    {
        return first_child_of(index) + 1;
    }

    template <typename UAlloc>
    using has_realloc_t = decltype(std::declval<UAlloc>().reallocate(std::declval<void*>(),
                                                                     std::declval<std::size_t>(),
                                                                     std::declval<std::size_t>()));

    template <typename UAlloc,
              typename std::enable_if<std::is_trivially_copyable<typename UAlloc::value_type>::value &&
                                          is_detected<has_realloc_t, UAlloc>::value,
                                      bool>::type = true>
    bool resize_heap(std::size_t new_size)
    {
        unique_ptr_t resized{alloc_.realloc(heap_.get(), new_size), heap_deleter_};
        if (resized)
        {
            // std::realloc specifies that, if non-null it returned from realloc, the previous pointer should
            // be discarded even if it is the same pointer value.
            (void) heap_.release();
            heap_.swap(resized);
            heap_capacity_ = new_size;
            return true;
        }
        else
        {
            return false;
        }
    }

    template <typename UAlloc,
              typename std::enable_if<std::is_trivially_copyable<typename UAlloc::value_type>::value &&
                                          !is_detected<has_realloc_t, UAlloc>::value,
                                      bool>::type = true>
    bool resize_heap(std::size_t new_size)
    {
        unique_ptr_t resized{alloc_.allocate(new_size), heap_deleter_};
        if (resized)
        {
            (void) memcpy(resized.get(), heap_.get(), sizeof(typename UAlloc::value_type) * heap_length_);
            heap_.swap(resized);
            heap_capacity_ = new_size;
            return true;
        }
        else
        {
            return false;
        }
    }

    // TODO: not-trivially-copyable resize_heap.

    // TODO: is trivially copyable, move constructable, etc.
    void swap(Value* lhs, Value* rhs)
    {
        Value tmp = *lhs;
        *lhs      = *rhs;
        *rhs      = tmp;
    }
};

}  // namespace pf17
}  // namespace cetl

std::ostream& operator<<(std::ostream& os, const int* value_pointer)
{
    if (nullptr == value_pointer)
    {
        os << "(null)";
    }
    else
    {
        os << *value_pointer;
    }
    return os;
}

int main()
{
    cetl::pf17::pmr::MallocResource            rr;
    cetl::pf17::pmr::polymorphic_allocator<int> allocator(&rr);
    cetl::pf17::Heap<int>                       a{allocator};
    a.reserve(20);
    const std::size_t initial_capacity = a.capacity();
    for (std::size_t i = 0; i < initial_capacity; ++i)
    {
        a.insert(static_cast<int>(i));
    }

    std::cout << "+---[bottom right-up]-------------+" << std::endl;
    for (auto i = a.rightmost_leaf(), end = a.tree_end(); i != end; --i)
    {
        std::cout << "node " << i.id() << " = " << *i << " (left " << i->first << ", right " << i->second << ")"
                  << std::endl;
    }

    std::cout << "+---[top down-left]-------------+" << std::endl;
    for (auto i = a.root(), end = a.tree_end(); i != end; i.left())
    {
        std::cout << "node " << i.id() << " = " << *i << " (left " << i->first << ", right " << i->second << ")"
                  << std::endl;
    }

    std::cout << "+---[top down-right]-------------+" << std::endl;
    for (auto i = a.root(), end = a.tree_end(); i != end; i.right())
    {
        std::cout << "node " << i.id() << " = " << *i << " (left " << i->first << ", right " << i->second << ")"
                  << std::endl;
    }

    std::cout << "is_heap? " << std::is_heap(a.begin(), a.end(), std::greater<int>()) << std::endl;
    return 0;
}
