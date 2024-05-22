/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_FUNCTION_H_INCLUDED
#define CETL_PMR_FUNCTION_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/pf17/cetlpf.hpp"
#include "cetl/unbounded_variant.hpp"

#include <cstddef>
#include <exception>
#include <functional>

namespace cetl
{
namespace pmr
{

template <typename Signature, std::size_t Footprint, bool IsPmr = false>
class function;

/// Internal implementation details. Not supposed to be used directly by the users of the library.
///
namespace detail
{

// 436C9E2B-96E3-4483-9D2B-32B5147A0314
using function_handler_typeid_t = cetl::
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    type_id_type<0x43, 0x6C, 0x9E, 0x2B, 0x96, 0xE3, 0x44, 0x83, 0x9D, 0x2B, 0x32, 0xB5, 0x14, 0x7A, 0x03, 0x14>;

/// @brief Provides an abstract interface for copyable functors.
///
template <typename Result, typename... Args>
class function_handler : public rtti_helper<function_handler_typeid_t>
{
public:
    virtual Result operator()(Args...) = 0;

};  // function_handler

// DCAAADD6-BC73-4E3C-85B7-E9473641E737
using functor_handler_typeid_t = cetl::
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    type_id_type<0xDC, 0xAA, 0xAD, 0xD6, 0xBC, 0x73, 0x4E, 0x3C, 0x85, 0xB7, 0xE9, 0x47, 0x36, 0x41, 0xE7, 0x37>;

template <typename Functor, typename Result, typename... Args>
class functor_handler final : public rtti_helper<functor_handler_typeid_t, function_handler<Result, Args...>>
{
public:
    explicit functor_handler(Functor functor)
        : functor_{functor}
    {
    }

    Result operator()(Args... args) override
    {
        return functor_(args...);
    }

private:
    Functor functor_;

};  // functor_handler

[[noreturn]] inline void throw_bad_function_call()
{
#if defined(__cpp_exceptions)
    throw std::bad_function_call{};
#else
    std::terminate();
#endif
}

template <typename Signature, std::size_t Footprint, bool IsPmr>
class function_base;
//
template <typename Result, typename... Args, std::size_t Footprint>
class function_base<Result(Args...), Footprint, false /*IsPmr*/>
{
public:
    function_base() noexcept                                 = default;
    ~function_base()                                         = default;
    function_base& operator=(const function_base& other)     = delete;
    function_base& operator=(function_base&& other) noexcept = delete;

    function_base(const function_base& other)
    {
        if (other.any_handler_.has_value())
        {
            any_handler_ = other.any_handler_;
            handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    function_base(function_base&& other) noexcept
    {
        if (other.any_handler_.has_value())
        {
            any_handler_       = std::move(other.any_handler_);
            handler_ptr_       = get_if<detail::function_handler<Result, Args...>>(&any_handler_);
            other.handler_ptr_ = nullptr;

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    // TODO: Add Callable constraint;
    template <typename Functor>
    function_base(Functor&& functor)
        : any_handler_{detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
    }

    // TODO: Add Callable constraint.
    template <typename Functor>
    function_base& operator=(Functor&& functor)
    {
        function_base tmp{std::forward<Functor>(functor)};
        tmp.swap(*this);
        return *this;
    }

    void swap(function_base& other) noexcept
    {
        any_handler_.swap(other.any_handler_);

        handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);
        other.handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&other.any_handler_);
    }

protected:
    // TODO: Invest (later) into exploiting `Copyable` & `Movable` template params of our `unbounded_variant` -
    // maybe we can make our `function_base` itself `Copyable` & `Movable` parametrized (aka `std::move_only_function`).
    unbounded_variant<Footprint, true, true, alignof(std::max_align_t), false /*IsPmr*/> any_handler_;
    detail::function_handler<Result, Args...>*                                           handler_ptr_{nullptr};
};
template <typename Result, typename... Args, std::size_t Footprint>
class function_base<Result(Args...), Footprint, true /*IsPmr*/>
{
public:
    function_base() noexcept
        : any_handler_{get_default_resource()}
    {
    }

    ~function_base()                                         = default;
    function_base& operator=(const function_base& other)     = delete;
    function_base& operator=(function_base&& other) noexcept = delete;

    explicit function_base(memory_resource* const mem_res)
        : any_handler_{mem_res}
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res, "");
    }

    function_base(const function_base& other)
        : any_handler_{other.any_handler_.get_memory_resource()}
    {
        if (other.any_handler_.has_value())
        {
            any_handler_ = other.any_handler_;
            handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    function_base(function_base&& other) noexcept
        : any_handler_{other.any_handler_.get_memory_resource()}
    {
        if (other.any_handler_.has_value())
        {
            any_handler_       = std::move(other.any_handler_);
            handler_ptr_       = get_if<detail::function_handler<Result, Args...>>(&any_handler_);
            other.handler_ptr_ = nullptr;

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    // TODO: Add Callable constraint.
    template <typename Functor>
    function_base(Functor&& functor)
        : any_handler_{get_default_resource(),
                       detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
    }

    // TODO: Add Callable constraint.
    template <typename Functor>
    function_base(memory_resource* const mem_res, Functor&& functor)
        : any_handler_{mem_res, detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
    }

    // TODO: Add Callable constraint.
    template <typename Functor>
    function_base& operator=(Functor&& functor)
    {
        function_base tmp{any_handler_.get_memory_resource(), std::forward<Functor>(functor)};
        tmp.swap(*this);
        return *this;
    }

    void swap(function_base& other) noexcept
    {
        any_handler_.swap(other.any_handler_);

        handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);
        other.handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&other.any_handler_);
    }

    CETL_NODISCARD memory_resource* get_memory_resource() const noexcept
    {
        return any_handler_.get_memory_resource();
    }

protected:
    // TODO: Invest (later) into exploiting `Copyable` & `Movable` template params of our `unbounded_variant` -
    // maybe we can make our `function_base` itself `Copyable` & `Movable` parametrized (aka `std::move_only_function`).
    unbounded_variant<Footprint, true, true, alignof(std::max_align_t), true /*IsPmr*/> any_handler_;
    detail::function_handler<Result, Args...>*                                          handler_ptr_{nullptr};

}; // function_base

}  // namespace detail

template <typename Result, typename... Args, std::size_t Footprint, bool IsPmr>
class function<Result(Args...), Footprint, IsPmr> final
    : public detail::function_base<Result(Args...), Footprint, IsPmr>
{
    using base = detail::function_base<Result(Args...), Footprint, IsPmr>;

public:
    using base::base;
    using base::swap;
    using base::operator=;
    using result_type = Result;

    function() noexcept                 = default;
    function(const function& other)     = default;
    function(function&& other) noexcept = default;
    ~function()                         = default;

    function(std::nullptr_t) noexcept {};

    function& operator=(const function& other)
    {
        if (this != &other)
        {
            function(other).swap(*this);
        }
        return *this;
    }

    function& operator=(function&& other) noexcept
    {
        if (this != &other)
        {
            function(std::move(other)).swap(*this);
        }
        return *this;
    }

    function& operator=(std::nullptr_t) noexcept
    {
        base::any_handler_.reset();
        base::handler_ptr_ = nullptr;
        return *this;
    }

    explicit operator bool() const noexcept
    {
        CETL_DEBUG_ASSERT(base::any_handler_.has_value() == (nullptr != base::handler_ptr_), "");

        return base::any_handler_.has_value();
    }

    Result operator()(Args... args) const
    {
        CETL_DEBUG_ASSERT(base::any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != base::handler_ptr_, "");

        return base::handler_ptr_->operator()(args...);
    }

};  // function

template <typename Result, typename... Args, std::size_t Footprint, bool IsPmr = false>
inline void swap(function<Result(Args...), Footprint, IsPmr>& lhs,
                 function<Result(Args...), Footprint, IsPmr>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_FUNCTION_H_INCLUDED
