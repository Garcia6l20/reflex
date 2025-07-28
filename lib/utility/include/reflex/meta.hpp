#pragma once

#include <meta>
#include <ranges>
#include <vector>

#include <reflex/fixed_string.hpp>

namespace reflex::meta
{
using namespace std::meta;

constexpr std::meta::info null;

consteval auto member_named(meta::info R, std::string_view name, access_context ctx = access_context::current())
    -> meta::info
{
  for(std::meta::info field : members_of(R, ctx) | std::views::filter(has_identifier))
  {
    if(identifier_of(field) == name)
      return field;
  }
  return null;
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

consteval bool has_annotation(info R, info A)
{
  if(is_template(A))
  {
    return std::ranges::contains(annotations_of(R)                                      //
                                     | std::views::transform(meta::type_of)             //
                                     | std::views::filter(meta::has_template_arguments) //
                                     | std::views::transform(meta::template_of),
                                 A);
  }
  else if(is_type(A))
  {
    return std::ranges::contains(annotations_of(R) | std::views::transform(type_of), A);
  }
  else
  {
    return std::ranges::contains(annotations_of(R) | std::views::transform(constant_of), constant_of(A));
  }
}

consteval auto nonstatic_data_members_annotated_with(info R, info A, access_context ctx = access_context::current())
{
  return nonstatic_data_members_of(R, ctx) //
         | std::views::filter([A](auto member) { return has_annotation(member, A); });
}

consteval auto
    first_nonstatic_data_member_annotated_with(info R, info A, access_context ctx = access_context::current())
{
  auto members = meta::nonstatic_data_members_annotated_with(R, A, meta::access_context::unchecked());
  if(not members.empty())
  {
    return members.front();
  }
  else
  {
    return meta::null;
  }
}

consteval auto member_functions_annotated_with(info R, info A, access_context ctx = access_context::current())
{
  return members_of(R, ctx)                           //
         | std::views::filter(meta::is_user_declared) //
         | std::views::filter(meta::is_function)      //
         | std::views::filter([A](auto fn) { return has_annotation(fn, A); });
}

consteval auto first_member_function_annotated_with(info R, info A, access_context ctx = access_context::current())
{
  auto functions = meta::member_functions_annotated_with(R, A, meta::access_context::unchecked());
  if(not functions.empty())
  {
    return functions.front();
  }
  else
  {
    return meta::null;
  }
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

template <size_t N = size_t(-1)> consteval auto signature_of(meta::info R)
{
  std::vector sig{return_type_of(R)};
  sig.append_range(parameters_of(R) | std::views::transform(meta::type_of) | std::views::take(N));
  return sig;
}
} // namespace detail

template <meta::info R, size_t N = size_t(-1)> static constexpr auto signature_of()
{
  constexpr auto func = []
  {
    if constexpr(is_function(R))
    {
      return R;
    }
    else
    {
      using FnT = [:type_of(R):];
      return ^^FnT::operator();
    }
  }();
  constexpr auto wrapper = substitute(^^detail::signature_wrapper, detail::signature_of<N>(func));
  using wrapper_type     = [:wrapper:];
  return ^^typename wrapper_type::function_type;
}

template <meta::info R, meta::info Class, size_t N = size_t(-1)> static constexpr auto signature_of()
{
  constexpr auto wrapper = substitute(^^detail::signature_wrapper, detail::signature_of<N>(R));
  using wrapper_type     = [:wrapper:];
  using class_type       = [:Class:];
  return ^^typename wrapper_type::template method_type<class_type>;
}

template <auto... chars> struct static_string_wrapper
{
  static constexpr char data[sizeof...(chars) + 1] = {chars..., '\0'};
  static constexpr auto size()
  {
    return sizeof...(chars);
  }
  static constexpr std::string_view view()
  {
    return {data, size()};
  }
  template <fixed_string prefix> static constexpr auto with_prefix()
  {
    return [&]<size_t... I>(std::index_sequence<I...>)
    { return static_string_wrapper<prefix[I]..., chars...>{}; }(std::make_index_sequence<prefix.size() - 1>());
  }
  template <fixed_string suffix> static constexpr auto with_suffix()
  {
    return [&]<size_t... I>(std::index_sequence<I...>)
    { return static_string_wrapper<chars..., suffix[I]...>{}; }(std::make_index_sequence<suffix.size() - 1>());
  }
  template <auto filter> static constexpr auto remove_if()
  {
    constexpr auto result_type = [] consteval
    {
      return substitute(template_of(^^static_string_wrapper),
                        view()                           //
                            | std::views::filter(filter) //
                            | std::views::transform([](char c) { return reflect_constant(c); }));
    }();
    using ResultT = [:result_type:];
    return ResultT{};
  }

  static constexpr fixed_string<size() + 1> to_fixed()
  {
    return data;
  }
};

template <meta::info R> static consteval auto static_identifier_wrapper_of()
{
  constexpr auto name = []
  {
    if(has_identifier(R))
    {
      return identifier_of(R);
    }
    else if(is_type(R))
    {
      return display_string_of(R);
    }
    std::unreachable();
  }();
  constexpr auto size = name.size();
  return [&]<size_t... I>(std::index_sequence<I...>)
  { return static_string_wrapper<name.data()[I]...>{}; }(std::make_index_sequence<name.size()>());
}

template <meta::info R> static consteval auto static_identifier_wrapper_type_of()
{
  constexpr auto wrapper = static_identifier_wrapper_of<R>();
  return type_of(^^wrapper);
}

} // namespace reflex::meta
