#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <string_view>
#endif

#include <reflex/constant.hpp>

REFLEX_EXPORT namespace reflex
{
  template <constant_string Name, typename T> struct named_type
  {
    using type                 = T;
    static constexpr auto name = Name;
  };

  template <constant_string Name, typename T> struct named_arg
  {
    using type                 = named_type<Name, T>;
    using value_type           = T;
    static constexpr auto name = Name;
    value_type            value;
  };

  template <constant_string Name> struct arg_name
  {
    static constexpr auto                              name = Name;
    template <typename T> constexpr named_arg<Name, T> operator=(T&& value) const
    {
      return {std::forward<T>(value)};
    }
  };

  namespace literals
  {
  template <constant_string Name> consteval auto operator""_na()
  {
    return arg_name<Name>{};
  }
  } // namespace literals
} // namespace reflex