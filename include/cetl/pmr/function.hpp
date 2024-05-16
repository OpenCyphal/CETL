/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_FUNCTION_H_INCLUDED
#define CETL_PMR_FUNCTION_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/unbounded_variant.hpp"

#include <cstddef>
#include <exception>
#include <functional>

namespace cetl
{
namespace pmr
{

template <typename Signature, std::size_t Footprint>
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
    virtual Result operator()(Args&&...) const = 0;

};  // function_handler

// DCAAADD6-BC73-4E3C-85B7-E9473641E737
using functor_handler_typeid_t = cetl::
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
    type_id_type<0xDC, 0xAA, 0xAD, 0xD6, 0xBC, 0x73, 0x4E, 0x3C, 0x85, 0xB7, 0xE9, 0x47, 0x36, 0x41, 0xE7, 0x37>;

template <typename Functor, typename Result, typename... Args>
class functor_handler final : public rtti_helper<functor_handler_typeid_t, function_handler<Result, Args...>>
{
public:
    explicit functor_handler(Functor&& functor) noexcept
    : functor_{std::move(functor)}
    {
    }

    Result operator()(Args&&... args) const override
    {
        return functor_(std::forward<Args>(args)...);
    }

private:
    Functor functor_;
};

[[noreturn]] inline void throw_bad_function_call()
{
#if defined(__cpp_exceptions)
    throw std::bad_function_call{};
#else
    std::terminate();
#endif
}

}  // namespace detail

template <typename Result, typename... Args, std::size_t Footprint>
class function<Result(Args...), Footprint>
{
public:
    using result_type = Result;

    /// \brief Constructs an empty `function` object.
    ///
    constexpr function() noexcept = default;
    constexpr function(nullptr_t) noexcept {};

    /// \brief Constructs a `function` object with a copy of the content of `other`.
    ///
    function(const function& other)
    {
        if (static_cast<bool>(other))
        {
            any_handler_ = other.any_handler_;
        }
    }

    /// \brief Constructs a `function` object with the content of `other` using move semantics.
    ///
    function(function&& other) noexcept
    {
        if (static_cast<bool>(other))
        {
            any_handler_ = std::move(other.any_handler_);
        }
    }

    // TODO: add Callable constraint
    template <typename Functor>
    function(Functor&& functor)
    : any_handler_{detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
    {
    }

    ~function() = default;

    function& operator=(const function& other)
    {
        function(other).swap(*this);
        return *this;
    }

    function& operator=(function&& other) noexcept
    {
        function(std::move(other)).swap(*this);
        return *this;
    }

    function& operator=(nullptr_t) noexcept
    {
        any_handler_.reset();
        return *this;
    }

    // TODO: add Callable constraint
    template <typename Functor>
    function& operator=(Functor&& functor)
    {
        function(std::forward<Functor>(functor)).swap(*this);
        return *this;
    }

    template <typename Functor>
    function& operator=(std::reference_wrapper<Functor> functor) noexcept
    {
        function(functor).swap(*this);
        return *this;
    }

    void swap(function& other) noexcept
    {
        any_handler_.swap(other.handler_);
    }

    explicit operator bool() const noexcept
    {
        return false;
    }

    Result operator()(Args... args) const
    {
        if (!any_handler_.has_value())
        {
            detail::throw_bad_function_call();
        }

        auto* func_handler_ptr = get_if<detail::function_handler<Result, Args...>>(&any_handler_);
        if (func_handler_ptr == nullptr)
        {
            detail::throw_bad_function_call();
        }

        return func_handler_ptr->operator()(std::forward<Args>(args)...);
    }

private:
    unbounded_variant<Footprint> any_handler_;

};  // function

template <typename Result, typename... Args, std::size_t Footprint>
inline void swap(function<Result(Args...), Footprint>& lhs, function<Result(Args...), Footprint>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_FUNCTION_H_INCLUDED
