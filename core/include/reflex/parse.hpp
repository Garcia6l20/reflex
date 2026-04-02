#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/exception.hpp>
#include <reflex/utils.hpp>

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <ranges>
#include <string_view>
#endif

REFLEX_EXPORT namespace reflex
{
  template <typename T> struct parser;

  template <typename T>
  concept Parsable = requires(std::string_view s) {
    { parser<T>{}(s) } -> std::same_as<T>;
  };

  template <typename T>
  concept parsable_c = requires(std::string_view s) {
    { parser<T>{}(s) } -> std::same_as<T>;
  };

  template <std::integral T> struct parser<T>
  {
    constexpr T operator()(std::string_view s) const
    {
      T value;

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
#ifdef __cpp_exceptions
        throw runtime_error("Failed to parse int from string: {}", s);
#else
        std::abort();
#endif
      }
      return value;
    }
  };

  template <std::floating_point T> struct parser<T>
  {
    constexpr T operator()(std::string_view s) const
    {
      T value;
      auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
      if(ec != std::errc())
      {
#ifdef __cpp_exceptions
        throw runtime_error(std::format("Failed to parse float from string: {}", s));
#else
        std::abort();
#endif
      }
      return value;
    }
  };

  template <> struct parser<std::string>
  {
    constexpr std::string operator()(std::string_view s) const
    {
      return std::string(s);
    }
  };

  template <> struct parser<std::string_view>
  {
    constexpr std::string_view operator()(std::string_view s) const
    {
      return s;
    }
  };

  template <> struct parser<bool>
  {
    constexpr bool operator()(std::string_view s) const
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
#ifdef __cpp_exceptions
        throw runtime_error("Failed to parse bool from string: {}", s);
#else
        std::abort();
#endif
      }
    }
  };

  template <enum_c E> struct parser<E>
  {
    constexpr E operator()(std::string_view s) const
    {
      template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
      {
        if(identifier_of(e) == s)
        {
          return extract<E>(e);
        }
      }
#ifdef __cpp_exceptions
      throw runtime_error("Failed to parse enum from string: {}", s);
#else
      std::abort();
#endif
    }
  };

  template <Parsable T> constexpr T parse(std::string_view s)
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
