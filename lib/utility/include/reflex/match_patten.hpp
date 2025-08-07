#pragma once

#include <utility>

namespace reflex
{
template <class... Ts> struct match_pattern : Ts...
{
  using Ts::operator()...;
};
template <class... Ts> match_pattern(Ts...) -> match_pattern<Ts...>;

namespace patterns
{
template <auto return_value>
inline constexpr auto unreachable_r = [](auto&&...)
{
  std::unreachable();
  return return_value;
};
inline constexpr auto unreachable = [](auto&&...) { std::unreachable(); };
} // namespace patterns
} // namespace reflex
