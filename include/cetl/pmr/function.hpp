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
#include <type_traits>

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

}  // namespace detail

template <typename Result, typename... Args, std::size_t Footprint, bool IsPmr>
class function<Result(Args...), Footprint, IsPmr> final
{
    template <typename Functor,
              bool = (!std::is_same<std::remove_cv<std::remove_reference<Functor>>, function>::value) &&
                     // TODO: Uncomment when we have `cetl::is_invocable_v`.
                     true /* std::is_invocable_v<Functor, Args...> */>
    struct IsCallable;
    template <typename Functor>
    struct IsCallable<Functor, false>
    {
        static constexpr bool value = false;
    };
    template <typename Functor>
    struct IsCallable<Functor, true>
    {
        static constexpr bool value =
            std::is_void<Result>::value ||
            // TODO: Uncomment when we have `cetl::invoke_result_t`.
            true /* std::is_convertible<std::invoke_result_t<Functor, Args...>, Result>::value */;
    };
    template <class Functor>
    using EnableIfLvIsCallable = std::enable_if_t<IsCallable<Functor&>::value>;

public:
    using result_type = Result;

    function() noexcept = default;
    ~function()         = default;

    function(std::nullptr_t) noexcept {};

    explicit function(memory_resource* const mem_res)
        : any_handler_{mem_res}
    {
        static_assert(IsPmr, "Cannot use memory resource with non-PMR function.");

        CETL_DEBUG_ASSERT(nullptr != mem_res, "");
    }

    function(const function& other)
        : any_handler_{other.any_handler_}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
    }

    function(function&& other) noexcept
        : any_handler_{std::move(other.any_handler_)}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
        other.handler_ptr_ = nullptr;
    }

    template <typename Functor, typename = EnableIfLvIsCallable<Functor>>
    function(Functor functor)
        : any_handler_{detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
    }

    template <typename Functor, typename = EnableIfLvIsCallable<Functor>>
    function(memory_resource* const mem_res, Functor&& functor)
        : any_handler_{mem_res, detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
        static_assert(IsPmr, "Cannot use memory resource with non-PMR function.");
    }

    template <typename Functor, typename = EnableIfLvIsCallable<Functor>>
    function& operator=(Functor&& functor)
    {
        any_handler_ = detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)};
        handler_ptr_ = get_if<handler_t>(&any_handler_);
        return *this;
    }

    function& operator=(const function& other)
    {
        if (this != &other)
        {
            any_handler_ = other.any_handler_;
            handler_ptr_ = get_if<handler_t>(&any_handler_);
        }
        return *this;
    }

    function& operator=(function&& other) noexcept
    {
        if (this != &other)
        {
            any_handler_ = std::move(other.any_handler_);
            handler_ptr_ = get_if<handler_t>(&any_handler_);
            other.handler_ptr_ = nullptr;
        }
        return *this;
    }

    function& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    explicit operator bool() const noexcept
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value() == (nullptr != handler_ptr_), "");

        return any_handler_.has_value();
    }

    Result operator()(Args... args) const
    {
        if (!any_handler_.has_value())
        {
            detail::throw_bad_function_call();
        }

        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");

        return handler_ptr_->operator()(args...);
    }

    void reset() noexcept
    {
        any_handler_.reset();
        handler_ptr_ = nullptr;
    }

    void reset(memory_resource* const mem_res) noexcept
    {
        static_assert(IsPmr, "Cannot reset memory resource to non-PMR function.");

        any_handler_.reset(mem_res);
        handler_ptr_ = nullptr;
    }

    void swap(function& other) noexcept
    {
        any_handler_.swap(other.any_handler_);

        handler_ptr_       = get_if<handler_t>(&any_handler_);
        other.handler_ptr_ = get_if<handler_t>(&other.any_handler_);
    }

    /// True if the function is valueless b/c of an exception.
    ///
    /// Use `reset` method (or try assign a new value) to recover from this state.
    ///
    CETL_NODISCARD bool valueless_by_exception() const noexcept
    {
        return any_handler_.valueless_by_exception();
    }

    CETL_NODISCARD memory_resource* get_memory_resource() const noexcept
    {
        static_assert(IsPmr, "Cannot get memory resource from non-PMR function.");

        return any_handler_.get_memory_resource();
    }

private:
    using handler_t = detail::function_handler<Result, Args...>;
    // TODO: Invest (later) into exploiting `Copyable` & `Movable` template params of our `unbounded_variant` -
    // maybe we can make our `function` itself `Copyable` & `Movable` parametrized (aka
    // `std::move_only_function`).
    using any_handler_t =
        unbounded_variant<Footprint, true /*Copyable*/, true /*Movable*/, alignof(std::max_align_t), IsPmr>;

    any_handler_t any_handler_;
    handler_t*    handler_ptr_{nullptr};

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
