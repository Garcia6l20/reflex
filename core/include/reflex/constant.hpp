#pragma once

#ifndef REFLEX_MODULE
#define REFLEX_EXPORT

#include <format>
#include <string>
#include <vector>
#endif

#include <reflex/ctp/ctp.hpp>

REFLEX_EXPORT namespace reflex
{
  template <typename T> using constant = ctp::param<T>;

  struct constant_string : constant<std::string>
  {
    using constant<std::string>::constant;
    using constant<std::string>::operator=;
    using constant<std::string>::operator==;
    using constant<std::string>::operator<=>;

    consteval constant_string(ctp::param<const char*> str) : constant<std::string>(std::string(str))
    {}
  };

  template <constant V> struct constant_wrapper
  {
    static constexpr decltype(auto) value = *V;

    constexpr decltype(auto) operator*() const
    {
      return value;
    }
    constexpr decltype(auto) operator->() const
    {
      return std::addressof(value);
    }
  };

  template <constant V> constexpr auto cw = constant_wrapper<V>{};

  namespace literals
  {
  consteval constant_string operator""_sc(const char* data, std::size_t N)
  {
    return {
        std::string_view{data, N}
    };
  }
  } // namespace literals
}

REFLEX_EXPORT namespace std
{
  template <typename T, typename CharT>
  struct formatter<reflex::constant<T>, CharT>
      : std::formatter<typename reflex::constant<T>::type, CharT>
  {
    constexpr auto format(reflex::constant<T> const& cst, auto& ctx) const
    {
      return std::formatter<typename reflex::constant<T>::type, CharT>::format(cst.get(), ctx);
    }
  };
  template <typename CharT>
  struct formatter<reflex::constant_string, CharT>
      : std::formatter<std::basic_string_view<CharT>, CharT>
  {
    constexpr auto format(reflex::constant_string const& cst, auto& ctx) const
    {
      return std::formatter<std::basic_string_view<CharT>, CharT>::format(cst.get(), ctx);
    }
  };
} // namespace std
