#pragma once

#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <format>
#endif

#include <reflex/concepts.hpp>
#include <reflex/derive.hpp>

REFLEX_EXPORT namespace reflex
{
  constexpr struct __debug_tag
  {
  } Debug;

  namespace detail
  {
  template <typename T> struct debug_wrapper
  {
    T const& value;
  };
  } // namespace detail

  template <aggregate_c T> constexpr auto debug(T const& value)
  {
    return detail::debug_wrapper<T>{value};
  }
}

REFLEX_EXPORT namespace std
{
  template <typename T, typename CharT>
  struct formatter<reflex::detail::debug_wrapper<T>, CharT> : formatter<basic_string_view<CharT>>
  {
    template <typename FormatContext>
    auto format(reflex::detail::debug_wrapper<T> const& wrapper, FormatContext& ctx) const
    {
      decltype(auto) out = ctx.out();
      out++              = '{';
      bool first         = true;
      template for(constexpr auto& member : define_static_array(
                       nonstatic_data_members_of(^^T, std::meta::access_context::current())))
      {
        if(not first)
        {
          out++ = ',';
          out++ = ' ';
        }
        else
        {
          first = false;
        }
        auto const& member_value = wrapper.value.[:member:];
        using MemberType         = std::remove_cvref_t<decltype(member_value)>;
        if constexpr(std::formattable<MemberType, CharT>)
        {
          out = std::format_to(out, "{}: {}", identifier_of(member), member_value);
        }
        else if constexpr(reflex::aggregate_c<MemberType>)
        {
          out = std::format_to(out, "{}: {}", identifier_of(member), reflex::debug(member_value));
        }
        else
        {
          out = std::format_to(out, "{}: <unformattable type>", identifier_of(member));
        }
      }
      out++ = '}';
      return out;
    }
  };

  template <reflex::derives_c<reflex::derive_t<reflex::Debug>> T, typename CharT>
  struct formatter<T, CharT> : formatter<reflex::detail::debug_wrapper<T>, CharT>
  {
    template <typename FormatContext> auto format(T const& value, FormatContext& ctx) const
    {
      return formatter<reflex::detail::debug_wrapper<T>, CharT>{}.format(reflex::debug(value), ctx);
    }
  };
}
