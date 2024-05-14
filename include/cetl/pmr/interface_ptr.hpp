/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_INTERFACE_PTR_H_INCLUDED
#define CETL_PMR_INTERFACE_PTR_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/unbounded_variant.hpp"

#include <memory>

namespace cetl
{
namespace pmr
{

template <typename Interface>
class PmrInterfaceDeleter final
{
public:
    template <typename PmrAllocator>
    PmrInterfaceDeleter(const PmrAllocator& alloc, std::size_t count)
        : some_deallocator_{PmrRawDeallocator<PmrAllocator>{alloc, count}}
        , type_caster_{[](Interface* ptr) { return raw_cast<typename PmrAllocator::value_type>(ptr); }}
    {
    }

    template <typename Up>
    PmrInterfaceDeleter(const PmrInterfaceDeleter<Up>& other)
        : some_deallocator_{other.some_deallocator_}
        , type_caster_{[](Interface* ptr) { return raw_cast<Up>(ptr); }}
    {
    }

    void operator()(Interface* ptr) noexcept
    {
        if (nullptr != ptr)
        {
            if (auto deallocator = cetl::get_if<IRawDeallocator>(&some_deallocator_))
            {
                auto* const raw_ptr = type_caster_(ptr);
                deallocator->operator()(static_cast<void*>(raw_ptr));
            }
        }
    }

private:
    template <typename Up>
    friend class PmrInterfaceDeleter;

    template <typename Concrete>
    static void* raw_cast(Interface* ptr) noexcept
    {
        return static_cast<void*>(static_cast<Concrete*>(ptr));
    }

    // D4BB1B80-612D-4330-9807-E4B2E2D76220
    using IRawDeallocatorTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0xD4, 0xBB, 0x1B, 0x80, 0x61, 0x2D, 0x43, 0x30, 0x98, 0x07, 0xE4, 0xB2, 0xE2, 0xD7, 0x62, 0x20>;

    // 2BBEAF90-7CFF-4AB8-ACBF-7C582EE66414
    using PmrRawDeallocatorTypeIdType = cetl::
        // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
        type_id_type<0x2B, 0xBE, 0xAF, 0x90, 0x7C, 0xFF, 0x4A, 0xB8, 0xAC, 0xBF, 0x7C, 0x58, 0x2E, 0xE6, 0x64, 0x14>;

    class IRawDeallocator : public cetl::rtti_helper<IRawDeallocatorTypeIdType>
    {
    public:
        virtual void operator()(void* raw_ptr) = 0;
    };

    template <typename PmrAllocator>
    class PmrRawDeallocator final : public cetl::rtti_helper<PmrRawDeallocatorTypeIdType, IRawDeallocator>
    {
    public:
        PmrRawDeallocator(const PmrAllocator& alloc, std::size_t count)
            : alloc_{alloc}
            , count_{count}
        {
        }

        // IRawDeallocator

        void operator()(void* raw_ptr) override
        {
            using Concrete = typename PmrAllocator::value_type;

            if (nullptr != raw_ptr)
            {
                auto concrete_ptr = static_cast<Concrete*>(raw_ptr);
                concrete_ptr->~Concrete();
                alloc_.deallocate(concrete_ptr, count_);
            }
        }

        PmrAllocator alloc_;
        std::size_t  count_;

    };  // PmrRawDeallocator

    cetl::unbounded_variant<3 * sizeof(void*)> some_deallocator_;
    void* (*type_caster_)(Interface*);

};  // PmrInterfaceDeleter

template <typename Interface>
using InterfacePtr = std::unique_ptr<Interface, PmrInterfaceDeleter<Interface>>;

class InterfaceFactory final
{
public:
    ~InterfaceFactory() = delete;
    InterfaceFactory()  = delete;

    template <typename Interface, typename PmrAllocator, typename... Args>
    static InterfacePtr<Interface> make_unique(PmrAllocator alloc, Args&&... args)
    {
        using Concrete = typename PmrAllocator::value_type;

        InterfacePtr<Concrete> concrete_ptr{alloc.allocate(1), PmrInterfaceDeleter<Concrete>{alloc, 1}};
        if (nullptr != concrete_ptr)
        {
            alloc.construct(concrete_ptr.get(), std::forward<Args>(args)...);
        }

        return concrete_ptr;
    }

};  // InterfaceFactory

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_INTERFACE_PTR_H_INCLUDED
