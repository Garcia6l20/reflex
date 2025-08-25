#pragma once

#include <reflex/visit.hpp>

#include <exception>
#include <utility>
#include <variant>

namespace reflex
{
template <class... Ts> struct match : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> match(Ts...) -> match<Ts...>;

namespace patterns
{
inline constexpr auto unreachable = [](auto&&...) { std::unreachable(); };
inline constexpr auto abort       = [](auto&&...) { std::abort(); };
inline constexpr auto terminate   = [](auto&&...) { std::terminate(); };
inline constexpr auto ignore      = [](auto&&...) {};
inline constexpr auto make        = [](auto value) { return [value](auto&&...) { return auto(value); }; };
inline constexpr auto throw_      = [](auto value) { return [value](auto&&...) { throw value; }; };

template <typename T> inline constexpr auto forward = [](T&& value) { return std::forward<T>(value); };

template <typename T> inline constexpr auto throw_on = [](auto value) { return [value](T&&) { throw value; }; };

template <auto return_value>
inline constexpr auto unreachable_r = [](auto&&...)
{
  std::unreachable();
  return return_value;
};
template <auto return_value>
inline constexpr auto abort_r = [](auto&&...)
{
  std::abort();
  return return_value;
};
template <auto return_value>
inline constexpr auto terminate_r = [](auto&&...)
{
  std::terminate();
  return return_value;
};
template <auto return_value> inline constexpr auto ignore_r = [](auto&&...) { return return_value; };
} // namespace patterns

template <typename T, typename... Fns> inline constexpr decltype(auto) operator|(T&& v, match<Fns...>&& m)
{
  return visit(reflex_fwd(m), reflex_fwd(v));
}

template <typename... Ts, typename... Fns>
inline constexpr decltype(auto) operator|(std::tuple<Ts...>&& v, match<Fns...>&& m)
{
  return std::apply([&m](auto&&... values) { return visit(reflex_fwd(m), reflex_fwd(values)...); }, reflex_fwd(v));
}

} // namespace reflex
