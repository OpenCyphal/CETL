/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_FUNCTION_H_INCLUDED
#define CETL_PMR_FUNCTION_H_INCLUDED

#include "cetl/unbounded_variant.hpp"

#include <cstddef>
#include <exception>
#include <functional>
#include <type_traits>

namespace cetl
{
namespace pmr
{

template <typename Signature, std::size_t Footprint, typename Pmr = void>
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
    explicit functor_handler(const Functor& functor)
        : functor_{functor}
    {
    }

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

/// @brief A polymorphic function wrapper.
///
/// The class is similar to `std::function` but with the following differences:
/// - it allows to set a small object optimization footprint;
/// - it doesn't use c++ heap; if needed PMR-enabled version can be used to manage memory.
///
template <typename Result, typename... Args, std::size_t Footprint, typename Pmr>
class function<Result(Args...), Footprint, Pmr> final
{
    template <typename Functor,
              bool = (!std::is_same<std::remove_cv_t<std::remove_reference_t<Functor>>, function>::value) &&
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

    /// @brief Creates an empty function.
    ///
    template <typename PmrAlias = Pmr, typename = cetl::detail::EnableIfNotPmrT<PmrAlias>>
    function() noexcept
    {
    }

    /// @brief Destroys the std::function instance. If the std::function is not empty, its target is destroyed also.
    ///
    ~function() = default;

    /// @brief Creates an empty function with specific memory resource.
    ///
    template <typename PmrAlias = Pmr, typename = cetl::detail::EnableIfPmrT<PmrAlias>>
    explicit function(Pmr* const mem_res)
        : any_handler_{mem_res}
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res, "");
    }

    /// @brief Copies the target of `other` to the target of `*this`.
    ///
    /// If `other` is empty, `*this` will be empty right after the call too.
    /// For PMR-enabled functions, the memory resource is copied as well.
    ///
    /// Any failure during the copy operation will result in the "valueless by exception" state.
    ///
    function(const function& other)
        : any_handler_{other.any_handler_}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
    }

    /// @brief Moves the target of `other` to the target of `*this`.
    ///
    /// If other is empty, *this will be empty right after the call too.
    /// `other` is in a valid but unspecified state right after the call.
    /// For PMR-enabled functions, the memory resource is copied as well.
    ///
    /// Any failure during the move operation will result in the "valueless by exception" state.
    ///
    function(function&& other) noexcept
        : any_handler_{std::move(other.any_handler_)}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
        other.handler_ptr_ = nullptr;
    }

    /// @brief Initializes the target with `std::forward<Functor>(functor)`.
    ///
    /// Any failure during the construction will result in the "valueless by exception" state.
    ///
    template <typename Functor,
              typename FunctorDecay = std::decay_t<Functor>,
              typename              = EnableIfLvIsCallable<FunctorDecay>,
              typename PmrAlias     = Pmr,
              typename              = cetl::detail::EnableIfNotPmrT<PmrAlias>>
    function(Functor&& functor)
        : any_handler_{detail::functor_handler<FunctorDecay, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<handler_t>(&any_handler_)}
    {
    }

    /// @brief Initializes the target with `std::forward<Functor>(functor)` and specific memory resource.
    ///
    /// Any failure during the construction will result in the "valueless by exception" state.
    ///
    template <typename Functor,
              typename FunctorDecay = std::decay_t<Functor>,
              typename              = EnableIfLvIsCallable<FunctorDecay>,
              typename PmrAlias     = Pmr,
              typename              = cetl::detail::EnableIfPmrT<PmrAlias>>
    function(Pmr* const mem_res, Functor&& functor)
        : any_handler_{mem_res, detail::functor_handler<FunctorDecay, Result, Args...>{std::forward<Functor>(functor)}}
        , handler_ptr_{get_if<detail::function_handler<Result, Args...>>(&any_handler_)}
    {
    }

    /// @brief Sets the target with `std::forward<Functor>(functor)`
    ///
    /// Any failure during the set operations will result in the "valueless by exception" state.
    ///
    template <typename Functor,
              typename FunctorDecay = std::decay_t<Functor>,
              typename              = EnableIfLvIsCallable<FunctorDecay>>
    function& operator=(Functor&& functor)
    {
        any_handler_ = detail::functor_handler<FunctorDecay, Result, Args...>{std::forward<Functor>(functor)};
        handler_ptr_ = get_if<handler_t>(&any_handler_);
        return *this;
    }

    /// @brief Assigns a copy of target of `other`.
    ///
    /// Any failure during the assignment will result in the "valueless by exception" state.
    ///
    function& operator=(const function& other)
    {
        if (this != &other)
        {
            any_handler_ = other.any_handler_;
            handler_ptr_ = get_if<handler_t>(&any_handler_);
        }
        return *this;
    }

    /// @brief Moves the target of `other` to `*this`
    ///
    /// Any failure during the movement will result in the "valueless by exception" state.
    ///
    function& operator=(function&& other) noexcept
    {
        if (this != &other)
        {
            any_handler_       = std::move(other.any_handler_);
            handler_ptr_       = get_if<handler_t>(&any_handler_);
            other.handler_ptr_ = nullptr;
        }
        return *this;
    }

    /// @brief Drops the current target. `*this` is empty after the call.
    ///
    function& operator=(std::nullptr_t) noexcept
    {
        reset();
        return *this;
    }

    /// @brief Checks whether `*this` stores a callable function target, i.e. is not empty.
    ///
    explicit operator bool() const noexcept
    {
        CETL_DEBUG_ASSERT(any_handler_.has_value() == (nullptr != handler_ptr_), "");

        return any_handler_.has_value();
    }

    /// @brief Invokes the stored callable function target with the parameters args.
    ///
    /// @param args Arguments to pass to the stored callable function target.
    /// @return None if `Result` is `void`. Otherwise the return value of the invocation of the stored callable object.
    /// @throws `std::bad_function_call` if `*this` does not store a callable function target, i.e. `!*this == true`.
    ///
    Result operator()(Args... args) const
    {
        if (!any_handler_.has_value())
        {
            detail::throw_bad_function_call();
        }

        CETL_DEBUG_ASSERT(nullptr != handler_ptr_, "");

        return handler_ptr_->operator()(args...);
    }

    /// @brief Resets the current target. `*this` is empty after the call.
    ///
    void reset() noexcept
    {
        any_handler_.reset();
        handler_ptr_ = nullptr;
    }

    /// @brief Resets the current target, and sets specific memory resource. `*this` is empty after the call.
    ///
    template <typename PmrAlias = Pmr, typename = cetl::detail::EnableIfPmrT<PmrAlias>>
    void reset(Pmr* const mem_res) noexcept
    {
        any_handler_.reset(mem_res);
        handler_ptr_ = nullptr;
    }

    /// @brief Swaps the contents of `*this` and `other`.
    ///
    /// Any failure during the swap operation could result in the "valueless by exception" state,
    /// and depending on which stage of swapping the failure happened
    /// it could affect (invalidate) either of `*this` or `other` function.
    /// Use `valueless_by_exception()` method to check if a function is in such failure state,
    /// and `reset` (or `reset(Pmr*)`) method (or assign a new value) to recover from it.
    ///
    void swap(function& other) noexcept
    {
        any_handler_.swap(other.any_handler_);

        handler_ptr_       = get_if<handler_t>(&any_handler_);
        other.handler_ptr_ = get_if<handler_t>(&other.any_handler_);
    }

    /// True if the function is valueless b/c of an exception or OOM.
    ///
    /// Use `reset` method (or try assign a new value) to recover from this state.
    ///
    CETL_NODISCARD bool valueless_by_exception() const noexcept
    {
        return any_handler_.valueless_by_exception();
    }

    /// @brief Gets current memory resource in use by the function.
    ///
    template <typename PmrAlias = Pmr, typename = cetl::detail::EnableIfPmrT<PmrAlias>>
    CETL_NODISCARD Pmr* get_memory_resource() const noexcept
    {
        return any_handler_.get_memory_resource();
    }

private:
    using handler_t = detail::function_handler<Result, Args...>;
    // TODO: Invest (later) into exploiting `Copyable` & `Movable` template params of our `unbounded_variant` -
    // maybe we can make our `function` itself `Copyable` & `Movable` parametrized (aka
    // `std::move_only_function`).
    using any_handler_t = unbounded_variant<Footprint, true /*Copy*/, true /*Move*/, alignof(std::max_align_t), Pmr>;

    any_handler_t any_handler_;
    handler_t*    handler_ptr_{nullptr};

};  // function

}  // namespace pmr
}  // namespace cetl

namespace std
{

/// @brief Overloads the `std::swap` algorithm for `cetl::pmr::function`.
/// Exchanges the state of `lhs` with that of `rhs`. Effectively calls `lhs.swap(rhs)`.
///
/// Any failure during the swap operation could result in the "valueless by exception" state,
/// and depending on which stage of swapping the failure happened
/// it could affect (invalidate) either of `lhs` or `rhs` function.
/// Use `valueless_by_exception()` method to check if a function is in such failure state,
/// and `reset` (or `reset(Pmr*)`) method (or assign a new value) to recover from it.
///
template <typename Result, typename... Args, std::size_t Footprint, typename Pmr = void>
void swap(cetl::pmr::function<Result(Args...), Footprint, Pmr>& lhs,
          cetl::pmr::function<Result(Args...), Footprint, Pmr>& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace std

#endif  // CETL_PMR_FUNCTION_H_INCLUDED
