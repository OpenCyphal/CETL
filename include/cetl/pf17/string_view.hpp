/// @file
/// Defines the C++17 std::variant type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_STRING_VIEW_HPP_INCLUDED
#define CETL_PF17_STRING_VIEW_HPP_INCLUDED

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <exception>  // We need this even if exceptions are disabled for std::terminate.
#include <stdexcept>
#include <string>

namespace cetl
{
namespace pf17
{

class string_view
{
public:
    // types
    using value_type      = char;
    using pointer         = char*;
    using const_pointer   = const char*;
    using reference       = const char&;
    using const_reference = const char&;
    using const_iterator  = const char*;
    using iterator        = const_iterator;
    using size_type       = std::size_t;

    enum : size_type
    {
        npos = static_cast<size_type>(-1)
    };

    constexpr string_view() noexcept
        : data_{nullptr}
        , size_{0}
    {
    }

    /// No lint and Sonar cpp:S1709 b/c this is an intentional implicit conversion.
    ///
    /// NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    string_view(const char* const str)  // NOSONAR cpp:S1709
        : data_{str}
        , size_{std::char_traits<char>::length(str)}
    {
    }

    /// No lint and Sonar cpp:S1709 b/c this is an intentional implicit conversion.
    ///
    /// NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    string_view(const std::string& str)  // NOSONAR cpp:S1709
        : data_{str.data()}
        , size_{str.size()}
    {
    }

    constexpr string_view(const char* const str, const size_type size)
        : data_{str}
        , size_{size}
    {
    }

    constexpr string_view(std::nullptr_t) = delete;

    constexpr size_type size() const noexcept
    {
        return size_;
    }

    constexpr size_type length() const noexcept
    {
        return size_;
    }

    constexpr size_type max_size() const noexcept
    {
        return (npos - sizeof(size_type) - sizeof(void*)) / sizeof(value_type) / 4;
    }

    constexpr bool empty() const noexcept
    {
        return size_ == 0;
    }

    constexpr const_reference operator[](const size_type pos) const
    {
        return data_[pos];
    }

    constexpr const_reference at(const size_type pos) const
    {
        if (pos >= size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("string_view::at");
#else
            std::terminate();
#endif
        }
        return data_[pos];
    }

    constexpr const_reference front() const
    {
        return data_[0];
    }

    constexpr const_reference back() const
    {
        return data_[size_ - 1];
    }

    constexpr const_pointer data() const noexcept
    {
        return data_;
    }

    constexpr const_iterator begin() const noexcept
    {
        return data_;
    }

    constexpr const_iterator end() const noexcept
    {
        return data_ + size_;
    }

    constexpr const_iterator cbegin() const noexcept
    {
        return data_;
    }

    constexpr const_iterator cend() const noexcept
    {
        return data_ + size_;
    }

    constexpr void remove_prefix(const size_type n)
    {
        const size_type to_remove = std::min(n, size_);
        data_ += to_remove;
        size_ -= to_remove;
    }

    constexpr void remove_suffix(const size_type n)
    {
        const size_type to_remove = std::min(n, size_);
        size_ -= to_remove;
    }

    constexpr void swap(string_view& sv) noexcept
    {
        const char* const tmp_data = data_;
        const size_type   tmp_size = size_;
        data_                      = sv.data_;
        size_                      = sv.size_;
        sv.data_                   = tmp_data;
        sv.size_                   = tmp_size;
    }

    size_type copy(char* const dest, const size_type count, const size_type pos = 0) const
    {
        if (pos > size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("string_view::copy");
#else
            std::terminate();
#endif
        }
        const size_type rcount = std::min(count, size_ - pos);
        std::copy_n(data_ + pos, rcount, dest);
        return rcount;
    }

    constexpr string_view substr(const size_type pos = 0, const size_type count = npos) const
    {
        if (pos > size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("string_view::substr");
#else
            std::terminate();
#endif
        }
        const size_type rcount = std::min(count, size_ - pos);
        return string_view(data_ + pos, rcount);
    }

    int compare(const string_view sv) const noexcept
    {
        const size_type rlen = std::min(size_, sv.size_);
        const int       cmp  = std::char_traits<char>::compare(data_, sv.data_, rlen);
        if (cmp != 0)
        {
            return cmp;
        }
        if (size_ == sv.size_)
        {
            return 0;
        }
        return size_ < sv.size_ ? -1 : 1;
    }

    friend bool operator==(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) == 0;
    }

    friend bool operator!=(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) != 0;
    }

    friend bool operator<(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) < 0;
    }

    friend bool operator>(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) > 0;
    }

    friend bool operator<=(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) <= 0;
    }

    friend bool operator>=(const string_view lhs, const string_view rhs) noexcept
    {
        return lhs.compare(rhs) >= 0;
    }

    constexpr size_type find(const char ch, const size_type pos = 0) const noexcept
    {
        if (pos >= size_)
        {
            return npos;
        }
        const const_iterator result = std::find(data_ + pos, data_ + size_, ch);
        return result != data_ + size_ ? static_cast<size_type>(result - data_) : npos;
    }

    constexpr size_type find(const string_view sv, const size_type pos = 0) const noexcept
    {
        if (pos > size_ || sv.size_ > size_ - pos)
        {
            return npos;
        }
        const const_iterator result = std::search(data_ + pos, data_ + size_, sv.data_, sv.data_ + sv.size_);
        return result != data_ + size_ ? static_cast<size_type>(result - data_) : npos;
    }

    constexpr bool starts_with(const string_view sv) const noexcept
    {
        return size_ >= sv.size_ && compare(0, sv.size_, sv) == 0;
    }

    constexpr bool ends_with(const string_view sv) const noexcept
    {
        return size_ >= sv.size_ && compare(size_ - sv.size_, sv.size_, sv) == 0;
    }

private:
    int compare(const size_type pos, const size_type count, const string_view sv) const noexcept
    {
        return substr(pos, count).compare(sv);
    }

    const char* data_;
    size_type   size_;

};  // string_view

// non-member functions

// swap
inline void swap(string_view& lhs, string_view& rhs) noexcept
{
    lhs.swap(rhs);
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_STRING_VIEW_HPP_INCLUDED
