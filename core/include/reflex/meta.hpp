#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <meta>
#endif

#include <reflex/views/cartesian_product.hpp>

namespace reflex::meta::detail
{
template <typename T> consteval bool has_call_operator()
{
  if constexpr(requires { &T::operator(); })
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
} // namespace reflex::meta::detail

REFLEX_EXPORT namespace reflex::meta
{
  using namespace std::meta;

  using std::meta::access_context;
  using std::meta::info;

  using vector           = std::vector<meta::info>;
  using initializer_list = std::initializer_list<meta::info>;

  constexpr std::meta::info null;

  /** @brief an helper type convertible to any type
   *
   * Useful for inspecting other types (ie.: deducing function templates argument count)
   */
  struct any_arg
  {
    template <typename T> constexpr operator T() const
    {
      return T{};
    }
  };

  consteval auto member_named(
      meta::info R, std::string_view name, access_context ctx = access_context::current())
      -> meta::info
  {
    for(std::meta::info field : members_of(R, ctx) | std::views::filter(has_identifier))
    {
      if(identifier_of(field) == name)
        return field;
    }
    return null;
  }

  template <std::size_t template_max_args = 16> consteval bool has_call_operator(meta::info R)
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
      auto type          = is_type(R) ? R : type_of(R);
      auto has_call_op_r = substitute(
          ^^detail::has_call_operator, {
                                           type});
      auto has_call_op = extract<bool (*)()>(has_call_op_r);
      return has_call_op();
    }
  }

  /** @brief functional detector
   *
   * Returns true on:
   *  - functions
   *  - function templates
   *  - functors/lambda
   */
  consteval bool is_functional(auto R)
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

  consteval auto remove_cvref(meta::info r) -> meta::info
  {
    return substitute(
        ^^std::remove_cvref_t, {
                                   r});
  }

  consteval auto tuple_for(std::ranges::range auto elems) -> meta::info
  {
    return substitute(
        ^^std::tuple, elems | std::views::transform(remove_cvref) | std::ranges::to<std::vector>());
  }

  consteval bool is_template_instance_of(info R, info T)
  {
    return has_template_arguments(R) and template_of(R) == T;
  }

  consteval auto make_template_tester(info T)
  {
    return [T](info R) consteval { return is_template_instance_of(decay(R), T); };
  }

  consteval bool is_subclass_of(
      info R, info C, access_context const& ctx = access_context::current())
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
    return annotations_of(R) | std::views::filter([A](auto R) {
             if(is_template(A))
             {
               return is_template_instance_of(type_of(R), A);
             }
             else if(is_type(A))
             {
               return decay(type_of(R)) == decay(A);
             }
             else
             {
               return constant_of(R) == constant_of(A);
             }
           });
  }

  template <typename AnnotationType> consteval decltype(auto) annotation_value_of_with(info R)
  {
    auto annotations = annotations_of_with(R, ^^AnnotationType);
    if(annotations.empty())
    {
      throw std::meta::exception("No such annotation", R);
    }
    else
    {
      return extract<AnnotationType const&>(constant_of(annotations.front()));
    }
  }

  consteval bool has_annotation(info R, info A)
  {
    return not std::ranges::empty(annotations_of_with(R, A));
  }

  consteval auto nonstatic_data_members_annotated_with(
      info R, info A, access_context ctx = access_context::current())
  {
    return nonstatic_data_members_of(R, ctx) //
         | std::views::filter([A](auto member) { return has_annotation(member, A); });
  }

  consteval auto first_nonstatic_data_member_annotated_with(
      info R, info A, access_context ctx = access_context::current())
  {
    auto members = meta::nonstatic_data_members_annotated_with(R, A, ctx);
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
    return members_of(R, ctx) | std::views::filter([](auto R) {
             return not is_constructor(R)
                and ((is_user_declared(R) and is_function(R)) or is_function_template(R));
           });
  }

  namespace detail
  {
  template <typename T> consteval auto call_operator() -> meta::info
  {
    return ^^T::operator();
  }
  } // namespace detail

  /** @brief the function behind a callable, or meta::null
   *
   * Handles a function, a pointer or reference to one, and a class type with a
   * call operator, which covers std::function and a lambda alike.
   *
   * A templated call operator yields null: it has no parameter types until it
   * is substituted, the same reason a function template is not reachable
   * through a reflection.
   */
  consteval auto callable_function_of(info R) -> info
  {
    const info type = is_type(R) ? R : type_of(R);
    if(is_function_type(type))
    {
      return type;
    }
    const info stripped = remove_pointer(remove_reference(type));
    if(is_function_type(stripped))
    {
      return stripped;
    }
    if(is_class_type(type) and meta::has_call_operator(type))
    {
      const info call = extract<info (*)()>(substitute(^^detail::call_operator, {type}))();
      return is_function(call) ? call : meta::null;
    }
    return meta::null;
  }

  /** @brief every function declared in @p R under the identifier @p name
   *
   * @p R may be a namespace or a class type. An overloaded name cannot be
   * reflected directly, since `^^f` is ill-formed when `f` names more than one
   * function, so going through the enclosing scope is the only way to reach the
   * whole set.
   *
   * Function templates are not returned: `is_function` is false for them, and a
   * template has no parameter types to match against until it is substituted.
   */
  consteval auto functions_named(
      info R, std::string_view name, access_context ctx = access_context::current())
      -> std::vector<info>
  {
    return members_of(R, ctx)                                                     //
         | std::views::filter([](info m) { return is_function(m); })              //
         | std::views::filter(has_identifier)                                     //
         | std::views::filter([name](info m) { return identifier_of(m) == name; }) //
         | std::ranges::to<std::vector<info>>();
  }

  /** @brief the parameter types of a function, without its return type
   *
   * detail::signature_of returns the return type first, which is the wrong
   * shape whenever the parameters are what is being matched.
   */
  consteval auto parameter_types_of(info R) -> std::vector<info>
  {
    return parameters_of(R)                     //
         | std::views::transform(meta::type_of) //
         | std::ranges::to<std::vector<info>>();
  }

  consteval auto member_functions_annotated_with(
      info R, info A, access_context ctx = access_context::current())
  {
    return member_functions_of(R, ctx) //
         | std::views::filter([A](auto fn) { return has_annotation(fn, A); });
  }

  consteval auto first_member_function_annotated_with(
      info R, info A, access_context ctx = access_context::current())
  {
    auto functions = meta::member_functions_annotated_with(R, A, ctx);
    if(not functions.empty())
    {
      return functions.front();
    }
    else
    {
      return meta::null;
    }
  }

  consteval bool has_explicit_constructor(
      meta::info R, std::initializer_list<meta::info> args,
      meta::access_context ctx = meta::access_context::current())
  {
    if(not is_class_type(R))
    {
      return false;
    }
    else
    {
      return std::ranges::any_of(members_of(R, ctx), [&](auto M) { //
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
    return members_of_r(I, ctx)                                                     //
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

  template <std::size_t N = std::size_t(-1)> consteval auto signature_of(meta::info R)
  {
    std::vector sig{return_type_of(R)};
    sig.append_range(parameters_of(R) | std::views::transform(meta::type_of) | std::views::take(N));
    return sig;
  }
  } // namespace detail

  template <meta::info R, std::size_t N = std::size_t(-1)> constexpr auto signature_of()
  {
    constexpr auto func = [] {
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

  template <meta::info R, meta::info Class, std::size_t N = std::size_t(-1)>
  constexpr auto signature_of()
  {
    constexpr auto wrapper = substitute(^^detail::signature_wrapper, detail::signature_of<N>(R));
    using wrapper_type     = [:wrapper:];
    using class_type       = [:Class:];
    return ^^typename wrapper_type::template method_type<class_type>;
  }

  template <typename E>
    requires std::is_enum_v<E>
  constexpr std::string enum_to_string(E value)
  {
    std::string result = "<unnamed>";
    template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
    {
      if(value == [:e:])
      {
        result = std::string(identifier_of(e));
      }
    }
    return result;
  }

  template <typename E>
    requires std::is_enum_v<E>
  constexpr std::optional<E> string_to_enum(std::string_view name)
  {
    std::optional<E> result = std::nullopt;
    template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
    {
      if(name == identifier_of(e))
      {
        result = [:e:];
      }
    }
    return result;
  }

  consteval bool is_alias_type(std::meta::info R)
  {
    return is_type(R) and (dealias(R) != R);
  }

  template <std::meta::reflection_range R = std::initializer_list<std::meta::info>>
  consteval bool eval_concept(std::meta::info concept_, R && args)
  {
    return extract<bool>(substitute(concept_, std::forward<R>(args)));
  }
} // namespace reflex::meta
