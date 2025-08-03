#pragma once

#include <format>
#include <variant>

#include <reflex/match_patten.hpp>

template <typename... Ts, typename CharT>
  requires(std::formattable<Ts, CharT> and ...)
struct std::formatter<std::variant<Ts...>, CharT>
{
  constexpr auto parse(auto& ctx)
  {
    auto it = ctx.begin();
    if(it == ctx.end())
      return it;
    while(it != ctx.end() && *it != '}')
    {
    }
    return it;
  }
  auto format(std::variant<Ts...> const& v, auto& ctx) const
  {
    return std::visit(
        reflex::match_pattern{
            [&ctx](std::monostate const&) { return std::format_to(ctx.out(), "<empty>"); },
            [&ctx]<typename T>(T const& v) { return std::formatter<T>{}.format(v, ctx); },
        },
        v);
  }
};
