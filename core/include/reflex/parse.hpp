#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/exception.hpp>
#include <reflex/constant.hpp>
#include <reflex/utils.hpp>

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#endif

REFLEX_EXPORT namespace reflex
{
  inline constexpr struct __parse_tag{} Parse;

  template <typename T> struct parser;

  template <typename T> using parse_result = std::expected<T, std::error_code>;

  template <typename T>
  concept parsable_c = requires(std::string_view s) {
    { parser<T>{}(s) } -> std::same_as<parse_result<T>>;
  };

  template <std::integral T> struct parser<T>
  {
    constexpr parse_result<T> operator()(std::string_view s) const noexcept
    {
      T value{};

      int base = 10;
      if(s.size() > 2 && s[0] == '0')
      {
        if(s[1] == 'x' || s[1] == 'X')
        {
          base = 16;
          s.remove_prefix(2);
        }
        else if(s[1] == 'b' || s[1] == 'B')
        {
          base = 2;
          s.remove_prefix(2);
        }
      }

      auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value, base);
      if(ec != std::errc())
      {
        return std::unexpected(std::make_error_code(ec));
      }
      if(ptr != s.data() + s.size())
      {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }
      return value;
    }
  };

  template <std::floating_point T> struct parser<T>
  {
    constexpr parse_result<T> operator()(std::string_view s) const noexcept
    {
      T value{};
      auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
      if(ec != std::errc())
      {
        return std::unexpected(std::make_error_code(ec));
      }
      if(ptr != s.data() + s.size())
      {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }
      return value;
    }
  };

  template <> struct parser<std::string>
  {
    constexpr parse_result<std::string> operator()(std::string_view s) const noexcept
    {
      return std::string(s);
    }
  };

  template <> struct parser<std::string_view>
  {
    constexpr parse_result<std::string_view> operator()(std::string_view s) const noexcept
    {
      return s;
    }
  };

  template <> struct parser<bool>
  {
    constexpr parse_result<bool> operator()(std::string_view s) const noexcept
    {
      static constexpr std::array true_values  = {"true", "yes", "on", "1"};
      static constexpr std::array false_values = {"false", "no", "off", "0"};
      if(std::ranges::contains(true_values, s))
      {
        return true;
      }
      else if(std::ranges::contains(false_values, s))
      {
        return false;
      }
      else
      {
        return std::unexpected(std::make_error_code(std::errc::invalid_argument));
      }
    }
  };

  template <parsable_c T, constant_string spec = "">
  constexpr parse_result<T> parse(std::string_view s) noexcept(noexcept(parser<T>{}(s)))
  {
    constexpr auto p = [] consteval {
      parser<T> p{};
      if constexpr (not spec->empty())
      {
        if constexpr (requires { p.parse(spec.get()); })
        {
          p.parse(spec.get());
        }
        else
        {
          static_assert(false, "parser does not take a format specifier");
        }
      }
      return p;
    }();
    return p(s);
  }

  template <parsable_c T, constant_string spec = ""> constexpr T parse_or(std::string_view s, T fallback)
  {
    auto parsed = parse<T, spec>(s);
    if(parsed)
    {
      return std::move(parsed).value();
    }
    return fallback;
  }

  template <parsable_c T, constant_string spec = "", typename OnError>
  constexpr T parse_or_else(std::string_view s, OnError && on_error)
  {
    auto parsed = parse<T, spec>(s);
    if(parsed)
    {
      return std::move(parsed).value();
    }
    return std::forward<OnError>(on_error)(parsed.error());
  }

  template <parsable_c T, constant_string spec = ""> T parse_or_throw(std::string_view s)
  {
    auto parsed = parse<T, spec>(s);
    if(parsed)
    {
      return std::move(parsed).value();
    }

    constexpr auto type_name = [] {
      if constexpr(has_identifier(^^T))
      {
        return identifier_of(^^T);
      }
      else
      {
        return display_string_of(^^T);
      }
    }();
    throw parse_error("Parsing '{}' as '{}' failed: {}", s, type_name, parsed.error().message());
  }

#ifdef REFLEX_MODULE
  // commonly used parsers
  extern template struct parser<std::int32_t>;
  extern template struct parser<std::int64_t>;
  extern template struct parser<std::uint32_t>;
  extern template struct parser<std::uint64_t>;
  extern template struct parser<float>;
  extern template struct parser<double>;
#endif
} // namespace reflex
