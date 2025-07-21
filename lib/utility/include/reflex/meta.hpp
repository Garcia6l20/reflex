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

namespace detail
{
template <typename Ret, typename... Args> struct signature_wrapper
{
  using function_type                         = Ret(Args...);
  template <typename Class> using method_type = Ret (Class::*)(Args...);
};
consteval auto signature_of(meta::info R)
{
  std::vector sig{return_type_of(R)};
  sig.append_range(parameters_of(R) | std::views::transform(meta::type_of));
  return sig;
}
} // namespace detail

template <meta::info R> static constexpr auto signature_of()
{
  constexpr auto wrapper = substitute(^^detail::signature_wrapper, detail::signature_of(R));
  using wrapper_type     = [:wrapper:];
  return ^^typename wrapper_type::function_type;
}

template <meta::info R, meta::info Class> static constexpr auto signature_of()
{
  constexpr auto wrapper = substitute(^^detail::signature_wrapper, detail::signature_of(R));
  using wrapper_type     = [:wrapper:];
  using class_type       = [:Class:];
  return ^^typename wrapper_type::template method_type<class_type>;
}

template <auto... chars> struct static_string_wrapper
{
  static constexpr char data[sizeof...(chars)] = {chars...};
  static constexpr auto size()
  {
    return sizeof...(chars);
  }
  static constexpr std::string_view view()
  {
    return {data, size()};
  }
};

template <meta::info R> static consteval auto static_identifier_wrapper_of()
{
  constexpr auto name     = identifier_of(R);
  constexpr auto get_char = []<size_t I, auto name>() constexpr { return name[I]; };
  constexpr auto size     = name.size();
  return [&]<size_t... I>(std::index_sequence<I...>)
  { return static_string_wrapper<name.data()[I]..., '\0'>{}; }(std::make_index_sequence<name.size()>());
}

template <meta::info R> static consteval auto static_identifier_wrapper_type_of()
{
  constexpr auto wrapper = static_identifier_wrapper_of<R>();
  return type_of(^^wrapper);
}

} // namespace reflex::meta
