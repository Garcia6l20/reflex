#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/concepts.hpp>

#ifndef REFLEX_MODULE
#include <format>
#endif

REFLEX_EXPORT namespace reflex
{
  constexpr bool is_in_range(int c, int low, int high)
  {
    return (c >= low) and (c <= high);
  }
  constexpr bool is_cntrl(int c) noexcept
  {
    return is_in_range(c, '\x00', '\x1f') //
        or c
        == '\x7f';
  }
  constexpr bool is_print(int c) noexcept
  {
    return is_in_range(c, '\x20', '\x7e');
  }
  constexpr bool is_graph(int c) noexcept
  {
    return is_in_range(c, '\x21', '\x7e');
  }
  constexpr bool is_blank(int c) noexcept
  {
    return c == '\x09' or c == '\x20';
  }
  constexpr bool is_space(int c) noexcept
  {
    return is_in_range(c, '\x09', '\x0d') //
        or c
        == '\x20';
  }
  constexpr bool is_upper(int c) noexcept
  {
    return is_in_range(c, '\x41', '\x5a');
  }
  constexpr bool is_lower(int c) noexcept
  {
    return is_in_range(c, '\x61', '\x7a');
  }
  constexpr bool is_alpha(int c) noexcept
  {
    return is_upper(c) //
        or is_lower(c);
  }
  constexpr bool is_digit(int c) noexcept
  {
    return is_in_range(c, '\x30', '\x39');
  }
  constexpr bool is_xdigit(int c) noexcept
  {
    return is_digit(c)                    //
        or is_in_range(c, '\x41', '\x46') //
        or is_in_range(c, '\x61', '\x66');
  }
  constexpr bool is_alphanum(int c) noexcept
  {
    return is_digit(c) or is_alpha(c);
  }
  constexpr bool is_punct(int c) noexcept
  {
    return is_in_range(c, '\x21', '\x2f') //
        or is_in_range(c, '\x3a', '\x40') //
        or is_in_range(c, '\x5b', '\x60') //
        or is_in_range(c, '\x7b', '\x7e');
  }

  constexpr int to_lower(int c) noexcept
  {
    if(is_upper(c))
    {
      return c + ('a' - 'A');
    }
    return c;
  }
  constexpr int to_upper(int c) noexcept
  {
    if(is_lower(c))
    {
      return c - ('a' - 'A');
    }
    return c;
  }

  template <typename... Ts>
  concept always_false = false;

  namespace detail
  {
  template <typename Like, typename T> struct const_like_s;

  template <typename Like, typename T>
    requires(not std::is_const_v<std::remove_reference_t<Like>>)
  struct const_like_s<Like, T>
  {
    using type = T;
  };

  template <typename Like, typename T>
    requires(std::is_const_v<std::remove_reference_t<Like>>)
  struct const_like_s<Like, T>
  {
    using type = std::add_const_t<T>;
  };
  } // namespace detail

  template <typename Like, typename T>
  using const_like_t = typename detail::const_like_s<Like, T>::type;

  constexpr std::string_view trim(std::string_view s) noexcept
  {
    while(!s.empty() and is_space(s.front()))
    {
      s.remove_prefix(1);
    }
    while(!s.empty() and is_space(s.back()))
    {
      s.remove_suffix(1);
    }
    return s;
  }

  constexpr std::string_view ltrim(std::string_view s) noexcept
  {
    while(!s.empty() and is_space(s.front()))
    {
      s.remove_prefix(1);
    }
    return s;
  }

  constexpr std::string_view rtrim(std::string_view s) noexcept
  {
    while(!s.empty() and is_space(s.back()))
    {
      s.remove_suffix(1);
    }
    return s;
  }

  // Natural order: a run of digits compares as a number, everything else case-insensitively, so
  // "PA2" comes before "PA10" where a lexicographic order puts "PA10" first. When one string is a
  // prefix of the other the shorter one compares less, as it does lexicographically.
  constexpr bool natural_less(std::string_view a, std::string_view b) noexcept
  {
    std::size_t i = 0;
    std::size_t j = 0;
    while(i < a.size() and j < b.size())
    {
      if(is_digit(a[i]) and is_digit(b[j]))
      {
        // Compare digit runs numerically, skipping leading zeros.
        std::size_t ia = i;
        std::size_t jb = j;
        while(ia < a.size() and is_digit(a[ia]))
        {
          ++ia;
        }
        while(jb < b.size() and is_digit(b[jb]))
        {
          ++jb;
        }
        auto       na = a.substr(i, ia - i);
        auto       nb = b.substr(j, jb - j);
        const auto za = na.find_first_not_of('0');
        const auto zb = nb.find_first_not_of('0');
        na.remove_prefix(za == std::string_view::npos ? na.size() : za);
        nb.remove_prefix(zb == std::string_view::npos ? nb.size() : zb);
        if(na.size() != nb.size())
        {
          return na.size() < nb.size();
        }
        if(na != nb)
        {
          return na < nb;
        }
        i = ia;
        j = jb;
      }
      else
      {
        const auto ca = to_lower(a[i]);
        const auto cb = to_lower(b[j]);
        if(ca != cb)
        {
          return ca < cb;
        }
        ++i;
        ++j;
      }
    }
    return (a.size() - i) < (b.size() - j);
  }

} // namespace reflex

REFLEX_EXPORT namespace std
{
  template <typename CharT> struct formatter<std::byte, CharT>
  {
    constexpr auto parse(auto& ctx)
    {
      return ctx.begin();
    }

    constexpr auto format(const std::byte& b, auto& ctx) const
    {
      return format_to(ctx.out(), "{:02x}h", to_integer<std::uint8_t>(b));
    }
  };
}
