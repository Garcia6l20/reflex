#pragma once

#include <reflex/visit.hpp>
#include <reflex/match.hpp>

#include <format>

namespace std
{

template <reflex::variant_c Var> struct formatter<Var> : formatter<std::string_view>
{
  template <typename FormatContext> auto format(Var const& v, FormatContext& ctx) const
  {
    return reflex::visit(
        reflex::match{
            [&ctx](const formattable<char> auto& value) {
              return format_to(ctx.out(), "{}", value);
            },
            [&ctx](const auto&) { return format_to(ctx.out(), "<unprintable>"); },
        },
        v);
  }
};

template <typename T, typename CharT>
  requires std::formattable<T, CharT>
struct formatter<std::optional<T>, CharT> : formatter<std::basic_string_view<CharT>, CharT>
{
  template <typename FormatContext>
  auto format(const std::optional<T>& opt, FormatContext& ctx) const
  {
    if(opt.has_value())
    {
      return format_to(ctx.out(), "{}", opt.value());
    }
    else
    {
      return format_to(ctx.out(), "<empty>");
    }
  }
};

} // namespace std