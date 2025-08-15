#pragma once

#include <algorithm>
#include <meta>
#include <ranges>
#include <vector>

#include <reflex/views/cartesian_product.hpp>

namespace reflex::meta
{
using namespace std::meta;

using vector           = std::vector<meta::info>;
using initializer_list = std::initializer_list<meta::info>;

constexpr std::meta::info null;

/** @brief an helper type convertible to any type
 *
 * Usefull for inspecting other types (ie.: deducing function templates argument count)
 */
struct any_arg
{
  template <typename T> constexpr operator T() const
  {
    return T{};
  }
};

namespace detail
{
template <typename T> consteval bool has_call_operator()
{
  static constexpr auto any_value = meta::any_arg{};

  if constexpr(requires { ^^T::operator(); })
  {
    return true;
  }
  else
  {
    if constexpr(requires { ^^T::template operator(); })
    {
      static_assert(is_function_template(^^T::template operator()));
      return true;
    }
  }
  return false;
}
} // namespace detail

template <size_t template_max_args = 16> consteval bool has_call_operator(meta::info R)
{
  if(is_function(R))
  {
    return false;
  }
  else if(is_function_template(R))
  {
    return false;
  }
  else if(is_template(R))
  {
    static constexpr auto items = std::array{^^any_arg, reflect_constant(any_arg{})};

    template for(constexpr auto n_args : std::views::iota(1uz, template_max_args))
    {
      // compute permutation between type-template-parameters and non-type-template-parameters
      for(const auto& p : items | views::cartesian_product<n_args>)
      {
        if(can_substitute(R, p))
        {
          return has_call_operator(substitute(R, p));
        }
      }
    }
    return false;
  }
  else
  {
    auto type = is_type(R) ? R : type_of(R);
    return extract<bool>(reflect_invoke(substitute(^^detail::has_call_operator,
                                                   {
                                                       type}),
                                        {}));
  }
}

/** @brief functional detector
 *
 * Returns true on:
 *  - functions
 *  - function templates
 *  - functors/lambda
 */
constexpr bool is_functional(auto R)
{
  if(is_function(R))
  {
    return true;
  }
  else if(is_function_template(R))
  {
    return true;
  }
  else if(has_call_operator(R))
  {
    return true;
  }
  return false;
}

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

consteval bool is_template_instance_of(info R, info T)
{
  return has_template_arguments(R) and template_of(R) == T;
}

consteval auto make_template_tester(info T)
{
  return [T](info R) consteval { return is_template_instance_of(decay(R), T); };
}

consteval bool is_subclass_of(info R, info C, access_context const& ctx = access_context::current())
{
  if(not is_type(R) or not is_class_type(R))
  {
    return false;
  }
  for(auto b : bases_of(R, ctx))
  {
    auto c = type_of(b);
    if(c == C or is_template_instance_of(c, C))
    {
      return true;
    }
    if(is_class_type(c) and is_subclass_of(c, C))
    {
      return true;
    }
  }
  return false;
}

consteval auto annotations_of_with(info R, info A)
{
  return annotations_of(R) | std::views::filter(
                                 [A](auto R)
                                 {
                                   if(is_template(A))
                                   {
                                     return is_template_instance_of(type_of(R), A);
                                   }
                                   else if(is_type(A))
                                   {
                                     return type_of(R) == A;
                                   }
                                   else
                                   {
                                     return constant_of(R) == constant_of(A);
                                   }
                                 });
}

consteval bool has_annotation(info R, info A)
{
  return not std::ranges::empty(annotations_of_with(R, A));
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

consteval auto member_functions_of(info R, access_context ctx = access_context::current())
{
  return members_of(R, ctx) | std::views::filter(
                                  [](auto R)
                                  {
                                    return not is_constructor(R) and
                                           ((is_user_declared(R) and is_function(R)) or is_function_template(R));
                                  });
}

consteval auto member_functions_annotated_with(info R, info A, access_context ctx = access_context::current())
{
  return member_functions_of(R, ctx) //
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

consteval bool has_explicit_constructor(meta::info                        R,
                                        std::initializer_list<meta::info> args,
                                        meta::access_context              ctx = meta::access_context::current())
{
  if(not is_class_type(R))
  {
    return false;
  }
  else
  {
    return std::ranges::any_of(members_of(R, ctx), [&](auto M) {//
      return is_constructor(M) and is_explicit(M) and is_constructible_type(R, args);
    });
  }
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

consteval bool is_structural_type(meta::info R)
{
  using std::ranges::all_of;
  const auto ctx = meta::access_context::unchecked();
  return is_scalar_type(R) or is_lvalue_reference_type(R) or
         is_class_type(R) and
             all_of(bases_of(R, ctx), [](info o) { return is_public(o) and is_structural_type(type_of(o)); }) and
             all_of(nonstatic_data_members_of(R, ctx),
                    [](info o)
                    {
                      return is_public(o) and not is_mutable_member(o) and
                             is_structural_type(remove_all_extents(type_of(o)));
                    });
}

} // namespace reflex::meta
