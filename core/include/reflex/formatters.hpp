#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/visit.hpp>

#ifndef REFLEX_MODULE
#include <format>
#endif

REFLEX_EXPORT namespace std
{
  template <reflex::variant_c Var> struct formatter<Var> : formatter<std::string_view>
  {
    template <typename FormatContext> auto format(Var const& v, FormatContext& ctx) const
    {
      return reflex::visit(
          [&]<typename T>(T const& value) {
            using U = std::decay_t<T>;
            if constexpr(formattable<U, char>)
            {
              return format_to(ctx.out(), "{}", value);
            }
            else
            {
              return format_to(ctx.out(), "<unprintable:{}>", display_string_of(dealias(^^U)));
            }
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