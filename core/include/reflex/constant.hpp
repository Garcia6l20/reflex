#pragma once

#ifndef REFLEX_MODULE
#define REFLEX_EXPORT

#include <reflex/ctp/ctp.hpp>

#include <string>
#include <vector>
#include <format>
#endif


REFLEX_EXPORT namespace reflex
{
    template <typename T>
    using constant = ctp::param<T>;

    using constant_string = constant<std::string>;

    template <typename T>
    using constant_span = constant<std::vector<T>>;

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
  template <>
  struct formatter<reflex::constant_string>
      : std::formatter<std::string_view, char>
  {
    constexpr auto format(reflex::constant_string const& str, auto& ctx) const
    {
      return std::formatter<std::string_view, char>::format(str.get(), ctx);
    }
  };
} // namespace std
