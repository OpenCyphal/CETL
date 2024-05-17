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
    explicit functor_handler(Functor&& functor)
        : functor_{std::move(functor)}
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

template <typename Result, typename... Args, std::size_t Footprint>
class function<Result(Args...), Footprint>
{
public:
    using result_type = Result;

    /// \brief Constructs an empty `function` object.
    ///
    constexpr function() noexcept = default;

    /// \brief Constructs an empty `function` object.
    ///
    function(std::nullptr_t) noexcept {};

    /// \brief Constructs a `function` object with a copy of the content of `other`.
    ///
    function(const function& other)
    {
        if (static_cast<bool>(other))
        {
            CETL_DEBUG_ASSERT(other.any_handler_.has_value(), "");

            any_handler_ = other.any_handler_;
            handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    /// \brief Constructs a `function` object with the content of `other` using move semantics.
    ///
    function(function&& other) noexcept
    {
        if (static_cast<bool>(other))
        {
            CETL_DEBUG_ASSERT(other.any_handler_.has_value(), "");

            any_handler_ = std::move(other.any_handler_);
            handler_ptr_ = get_if<detail::function_handler<Result, Args...>>(&any_handler_);

            CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
            CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
        }
    }

    // TODO: add Callable constraint
    template <typename Functor>
    function(Functor&& functor)
        : any_handler_{detail::functor_handler<Functor, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");
    }

    ~function() = default;

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
        any_handler_.reset();
        handler_ptr_ = nullptr;
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
        any_handler_.swap(other.any_handler_);
        std::swap(handler_ptr_, other.handler_ptr_);
    }

    explicit operator bool() const noexcept
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value() == (nullptr != handler_ptr_), "");

        return any_handler_.has_value();
    }

    Result operator()(Args... args) const
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value(), "");
        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");

        return handler_ptr_->operator()(args...);
    }

private:
    // TODO: Invest (later) into exploiting `Copyable` & `Movable` template params of our `unbounded_variant` -
    // maybe we can make our `function` itself `Copyable` & `Movable` parametrized (aka `std::move_only_function`).
    unbounded_variant<Footprint>               any_handler_;
    detail::function_handler<Result, Args...>* handler_ptr_{nullptr};

};  // function

template <typename Result, typename... Args, std::size_t Footprint>
inline void swap(function<Result(Args...), Footprint>& lhs, function<Result(Args...), Footprint>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_FUNCTION_H_INCLUDED
