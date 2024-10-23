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
#include <ios>
#include <stdexcept>
#include <string>

namespace cetl
{
namespace pf17
{

/// The class template basic_string_view describes an object that can refer to a constant contiguous sequence of CharT
/// with the first element of the sequence at position zero.
///
template <typename CharT, typename Traits = std::char_traits<CharT>>
class basic_string_view
{
public:
    // types
    using traits_type     = Traits;
    using value_type      = CharT;
    using pointer         = CharT*;
    using const_pointer   = const CharT*;
    using reference       = CharT&;
    using const_reference = const CharT&;
    using const_iterator  = const CharT*;
    using iterator        = const_iterator;
    using size_type       = std::size_t;
    using difference_type = std::ptrdiff_t;

    enum : size_type
    {
        npos = static_cast<size_type>(-1)
    };

    /// Default constructor. Constructs an empty basic_string_view. After construction, data() is equal to nullptr,
    /// and size() is equal to 0.
    constexpr basic_string_view() noexcept
        : data_{nullptr}
        , size_{0}
    {
    }

    /// Constructs a view of the null-terminated character string pointed to by s, not including the terminating null
    /// character.
    ///
    /// No lint and Sonar cpp:S1709 b/c this is an intentional implicit conversion.
    ///
    /// NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    basic_string_view(const CharT* const str)  // NOSONAR cpp:S1709
        : data_{str}
        , size_{traits_type::length(str)}
    {
    }

    /// No lint and Sonar cpp:S1709 b/c this is an intentional implicit conversion.
    ///
    /// NOLINTNEXTLINE(google-explicit-constructor, hicpp-explicit-conversions)
    template <typename Alloc>
    basic_string_view(const std::basic_string<CharT, Traits, Alloc>& str)  // NOSONAR cpp:S1709
        : data_{str.data()}
        , size_{str.size()}
    {
    }

    ///  Constructs a view of the first count characters of the character array starting with the element pointed by
    ///  str. str can contain null characters.
    ///
    constexpr basic_string_view(const CharT* const str, const size_type size)
        : data_{str}
        , size_{size}
    {
    }

    /// basic_string_view cannot be constructed from nullptr.
    ///
    constexpr basic_string_view(std::nullptr_t) = delete;

    /// Returns the number of CharT elements in the view, i.e. `std::distance(begin(), end())`.
    ///
    constexpr size_type size() const noexcept
    {
        return size_;
    }

    /// Returns the number of CharT elements in the view, i.e. `std::distance(begin(), end())`.
    ///
    constexpr size_type length() const noexcept
    {
        return size_;
    }

    /// The largest possible number of char-like objects that can be referred to by a basic_string_view.
    ///
    constexpr size_type max_size() const noexcept
    {
        return (npos - sizeof(size_type) - sizeof(void*)) / sizeof(value_type) / 4;
    }

    /// Checks if the view has no characters, i.e. whether `size() == 0`.
    ///
    constexpr bool empty() const noexcept
    {
        return size_ == 0;
    }

    /// Returns a const reference to the character at specified location `pos`.
    ///
    constexpr const_reference operator[](const size_type pos) const
    {
        return data_[pos];
    }

    /// Returns a const reference to the character at specified location `pos`.
    ///
    /// Bounds checking is performed, exception of type `std::out_of_range` will be thrown on invalid access.
    ///
    constexpr const_reference at(const size_type pos) const
    {
        if (pos >= size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("basic_string_view::at");
#else
            std::terminate();
#endif
        }
        return data_[pos];
    }

    /// Returns reference to the first character in the view. The behavior is undefined if `empty() == true`.
    ///
    constexpr const_reference front() const
    {
        return data_[0];
    }

    /// Returns reference to the last character in the view. The behavior is undefined if `empty() == true`.
    ///
    constexpr const_reference back() const
    {
        return data_[size_ - 1];
    }

    /// Returns a pointer to the underlying character array.
    ///
    /// The pointer is such that the range `[data(), data() + size())` is valid and the values in it correspond to the
    /// values of the view.
    ///
    constexpr const_pointer data() const noexcept
    {
        return data_;
    }

    /// Returns an iterator to the first character of the view.
    ///
    constexpr const_iterator begin() const noexcept
    {
        return data_;
    }

    /// Returns an iterator to the character following the last character of the view.
    ///
    /// This character acts as a placeholder, attempting to access it results in undefined behavior.
    ///
    constexpr const_iterator end() const noexcept
    {
        return data_ + size_;
    }

    /// Returns a constant iterator to the first character of the view.
    ///
    constexpr const_iterator cbegin() const noexcept
    {
        return data_;
    }

    /// Returns a constant iterator to the character following the last character of the view.
    ///
    /// This character acts as a placeholder, attempting to access it results in undefined behavior.
    ///
    constexpr const_iterator cend() const noexcept
    {
        return data_ + size_;
    }

    /// Moves the start of the view forward by n characters.
    ///
    /// The behavior is undefined if `n > size()`.
    ///
    constexpr void remove_prefix(const size_type n)
    {
        const size_type to_remove = std::min(n, size_);
        data_ += to_remove;
        size_ -= to_remove;
    }

    /// Moves the end of the view back by n characters.
    ///
    /// The behavior is undefined if `n > size()`.
    ///
    constexpr void remove_suffix(const size_type n)
    {
        const size_type to_remove = std::min(n, size_);
        size_ -= to_remove;
    }

    /// Exchanges the view with that of `sv`.
    ///
    constexpr void swap(basic_string_view& sv) noexcept
    {
        const CharT* const tmp_data = data_;
        const size_type    tmp_size = size_;
        data_                       = sv.data_;
        size_                       = sv.size_;
        sv.data_                    = tmp_data;
        sv.size_                    = tmp_size;
    }

    /// Copies the substring [pos, pos + rcount) to the character array pointed to by dest, where rcount is the smaller
    /// of count and size() - pos.
    ///
    /// Equivalent to `Traits::copy(dest, data() + pos, rcount)`.
    ///
    size_type copy(CharT* const dest, const size_type count, const size_type pos = 0) const
    {
        if (pos > size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("basic_string_view::copy");
#else
            std::terminate();
#endif
        }
        const size_type rcount = std::min(count, size_ - pos);
        std::copy_n(data_ + pos, rcount, dest);
        return rcount;
    }

    /// Returns a view of the substring `[pos, pos + rlen)`, where `rlen` is the smaller of `count` and `size() - pos`.
    ///
    constexpr basic_string_view substr(const size_type pos = 0, const size_type count = npos) const
    {
        if (pos > size_)
        {
#if defined(__cpp_exceptions)
            throw std::out_of_range("basic_string_view::substr");
#else
            std::terminate();
#endif
        }
        const size_type rcount = std::min(count, size_ - pos);
        return basic_string_view(data_ + pos, rcount);
    }

    /// Compares two character sequences.
    ///
    /// @return Negative value if this view is less than the other character sequence, zero if the both character
    ///         sequences are equal, positive value if this view is greater than the other character sequence.
    ///
    int compare(const basic_string_view sv) const noexcept
    {
        const size_type rlen = std::min(size_, sv.size_);
        const int       cmp  = traits_type::compare(data_, sv.data_, rlen);
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

    /// Compares two views.
    ///
    friend bool operator==(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) == 0;
    }
    friend bool operator!=(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) != 0;
    }
    friend bool operator<(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) < 0;
    }
    friend bool operator>(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) > 0;
    }
    friend bool operator<=(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) <= 0;
    }
    friend bool operator>=(const basic_string_view lhs, const basic_string_view rhs) noexcept
    {
        return lhs.compare(rhs) >= 0;
    }

    /// Finds the first substring equal to the given character.
    ///
    constexpr size_type find(const CharT ch, const size_type pos = 0) const noexcept
    {
        if (pos >= size_)
        {
            return npos;
        }
        const const_iterator result = std::find(data_ + pos, data_ + size_, ch);
        return result != data_ + size_ ? static_cast<size_type>(result - data_) : npos;
    }

    /// Finds the first substring equal to the given character sequence.
    ///
    constexpr size_type find(const basic_string_view sv, const size_type pos = 0) const noexcept
    {
        if (pos > size_ || sv.size_ > size_ - pos)
        {
            return npos;
        }
        const const_iterator result = std::search(data_ + pos, data_ + size_, sv.data_, sv.data_ + sv.size_);
        return result != data_ + size_ ? static_cast<size_type>(result - data_) : npos;
    }

    /// Performs stream output on string view.
    ///
    friend std::basic_ostream<CharT, Traits>& operator<<(std::basic_ostream<CharT, Traits>& os,
                                                         const basic_string_view&           sv)
    {
        const auto sv_length = static_cast<std::streamsize>(sv.length());

        typename std::basic_ostream<CharT, Traits>::sentry s{os};
        if (s)
        {
            const std::streamsize width = os.width();

            if (sv_length < width)
            {
                auto pad = [&os, fill = os.fill()](std::streamsize padding_width) {
                    while (padding_width--)
                    {
                        os.put(fill);
                    }
                };

                const std::streamsize padding_len = width - sv_length;
                const bool            left        = os.flags() & std::basic_ios<CharT, Traits>::left;
                const bool            right       = os.flags() & std::basic_ios<CharT, Traits>::right;

                if (right)
                {
                    pad(padding_len);
                }
                os.write(sv.data(), sv_length);
                if (left)
                {
                    pad(padding_len);
                }
            }
            else
            {
                os.write(sv.data(), sv_length);
            }
        }
        os.width(0);
        return os;
    }

private:
    const CharT* data_;
    size_type    size_;

};  // basic_string_view

// non-member functions

/// Exchanges the given string views.
///
template <typename CharT>
void swap(basic_string_view<CharT>& lhs, basic_string_view<CharT>& rhs) noexcept
{
    lhs.swap(rhs);
}

using string_view = basic_string_view<char>;

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_STRING_VIEW_HPP_INCLUDED
