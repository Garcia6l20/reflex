export module reflex.core:formatters;

import :visit;
import :match;

import std;

export template <reflex::variant_c Var> struct std::formatter<Var> : std::formatter<std::string_view>
{
  template <typename FormatContext> auto format(Var const& v, FormatContext& ctx) const
  {
    return reflex::visit(
        reflex::match{
            [&ctx](const std::formattable<char> auto& value) {
              return std::format_to(ctx.out(), "{}", value);
            },
            [&ctx](const auto&) { return std::format_to(ctx.out(), "<unprintable>"); },
        },
        v);
  }
};
