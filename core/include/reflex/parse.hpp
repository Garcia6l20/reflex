#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/utils.hpp>

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <format>
#include <ranges>
#include <system_error>
#include <string>
#include <string_view>
#endif

REFLEX_EXPORT namespace reflex
{
  template <typename T> struct parser;

  template <typename T> using parse_result = std::expected<T, std::error_code>;

  template <typename T>
  concept Parsable = requires(std::string_view s) {
    { parser<T>{}(s) } -> std::same_as<parse_result<T>>;
  };

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

  template <enum_c E> struct parser<E>
  {
    constexpr parse_result<E> operator()(std::string_view s) const noexcept
    {
      template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
      {
        if(identifier_of(e) == s)
        {
          return extract<E>(e);
        }
      }
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }
  };

  template <Parsable T>
  constexpr parse_result<T> parse(std::string_view s) noexcept(noexcept(parser<T>{}(s)))
  {
    return parser<T>{}(s);
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
