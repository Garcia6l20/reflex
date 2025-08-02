#pragma once

#include <reflex/meta.hpp>

#include <array>
#include <print>
#include <ranges>
#include <tuple>

namespace reflex
{

namespace detail
{
consteval auto struct_to_tuple_type(meta::info type) -> meta::info
{
  constexpr auto ctx = meta::access_context::current();
  return meta::tuple_for(meta::nonstatic_data_members_of_r(type, ctx) //
                         | std::views::transform(meta::type_of)       //
                         | std::ranges::to<std::vector>());
}

template <typename To, typename From, meta::info... members>
constexpr auto struct_to_tuple_helper(From const& from) -> To
{
  return To(from.[:members:]...);
}

template <typename From> consteval auto get_struct_to_tuple_helper()
{
  using To = [:struct_to_tuple_type(^^From):];

  std::vector args = {^^To, ^^From};
  for(auto mem : meta::nonstatic_data_members_of_r(^^From, meta::access_context::unchecked()))
  {
    args.push_back(reflect_constant(mem));
  }

  return extract<To (*)(From const&)>(substitute(^^struct_to_tuple_helper, args));
}

template <typename From> constexpr auto struct_to_tuple(From const& from)
{
  return get_struct_to_tuple_helper<From>()(from);
}
} // namespace detail

template <typename From> constexpr decltype(auto) to_tuple(From const& from)
{
  constexpr auto R = decay(^^From);
  if constexpr(has_template_arguments(R) and template_of(R) == ^^std::tuple)
  {
    return from;
  }
  else
  {
    return detail::get_struct_to_tuple_helper<From>()(from);
  }
}
} // namespace reflex
