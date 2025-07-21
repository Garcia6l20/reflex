#pragma once

#include <meta>
#include <ranges>
#include <vector>

#include <reflex/fixed_string.hpp>

namespace reflex::meta
{
using namespace std::meta;

consteval auto member_named(meta::info R, std::string_view name) -> meta::info
{
  constexpr auto ctx = std::meta::access_context::current();
  for(std::meta::info field : members_of(R, ctx))
  {
    if(identifier_of(field) == name)
      return field;
  }
  return ^^void;
}

consteval auto remove_cvref(meta::info r) -> meta::info
{
  return substitute(^^std::remove_cvref_t,
                    {
                        r});
}

consteval auto tuple_for(std::ranges::range auto elems) -> meta::info
{
  return substitute(^^std::tuple, elems | std::views::transform(remove_cvref) | std::ranges::to<std::vector>());
}

template <info I, info Expected> consteval auto template_annotations_of()
{
  return annotations_of(I) //
         | std::views::filter(
               [&](auto A)
               {
                 auto AT = type_of(A);
                 return has_template_arguments(AT) and template_of(AT) == Expected;
               }) //
         | std::views::transform(std::meta::constant_of);
}

consteval auto template_annotations_of(info I, info Expected)
{
  return annotations_of(I) //
         | std::views::filter(
               [&](auto A)
               {
                 auto AT = type_of(A);
                 return has_template_arguments(AT) and template_of(AT) == Expected;
               }) //
         | std::views::transform(constant_of);
}

template <std::meta::info I> consteval auto fixed_identifier_of() noexcept
{
  constexpr auto name = identifier_of(I);
  return fixed_string<name.size()>(name.data());
}

consteval auto members_of_r(info I, access_context ctx) -> std::vector<info>
{
  std::vector<info> members;
  for(auto base : bases_of(I, ctx))
  {
    members.append_range(members_of_r(type_of(base), ctx));
  }
  members.append_range(members_of(I, ctx));
  return members;
}

consteval auto nonstatic_data_members_of_r(info I, access_context ctx) -> std::vector<info>
{
  return members_of_r(I, ctx)                                                       //
         | std::views::filter([](info II) { return is_nonstatic_data_member(II); }) //
         | std::ranges::to<std::vector<info>>();
}

} // namespace reflex::meta
