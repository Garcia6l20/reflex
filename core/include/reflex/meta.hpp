#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <meta>
#endif

#include <reflex/constant.hpp>
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

  /** @brief The member of @p R whose identifier is @p name.
   *
   * @param R the scope to search: a class type, or any scope `members_of` accepts
   * @param name the identifier to match, compared exactly
   * @param ctx the access context the lookup is performed from. Defaults to the
   *        caller's, so a private member is found from inside its own class and
   *        not from outside it. Pass `access_context::unchecked()` to ignore
   *        access entirely
   * @param recursive when true, and @p R is a class type, base classes are
   *        searched as well
   * @return the member, or `meta::null` when @p name names nothing reachable
   *
   * A recursive lookup takes @p R's own members first, so a member shadows one
   * of the same name in a base. Bases are then searched depth first in
   * declaration order and the first match wins, which means a name declared in
   * two unrelated bases resolves to the one declared first rather than being
   * reported as ambiguous.
   *
   * ```cpp
   * struct base    { int value; };
   * struct derived : base { int other; };
   *
   * static_assert(member_named(^^derived, "value") == meta::null);
   * static_assert(member_named(^^derived, "value", true) != meta::null);
   * ```
   */
  consteval auto member_named(meta::info       R,
                              std::string_view name,
                              access_context   ctx       = access_context::current(),
                              bool             recursive = false) -> meta::info
  {
    for(std::meta::info field : members_of(R, ctx) | std::views::filter(has_identifier))
    {
      if(identifier_of(field) == name)
        return field;
    }
    if(recursive and is_type(R) and is_class_type(R))
    {
      for(auto base : bases_of(R, ctx))
      {
        if(auto found = member_named(dealias(type_of(base)), name, ctx, true); found != null)
        {
          return found;
        }
      }
    }
    return null;
  }

  /** @brief The member of @p R whose identifier is @p name, searching bases.
   *
   * Spelling of the same lookup that puts @p recursive next to the name, for
   * the common case where the default access context is wanted. @p ctx stays a
   * default argument so it is still evaluated at the call site.
   */
  consteval auto member_named(meta::info       R,
                              std::string_view name,
                              bool             recursive,
                              access_context   ctx = access_context::current()) -> meta::info
  {
    return member_named(R, name, ctx, recursive);
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

  /** @brief the annotations of @p R, empty for a template or an unnamed bit-field
   *
   * `annotations_of` describes a type, type alias, variable, function, function
   * parameter, namespace, enumerator, base class relationship or non-static
   * data member, and throws on anything else. A walk over a class runs into two
   * of those: `members_of` hands back a member function template, a variable
   * template and a nested class template alongside the rest, and it hands back
   * an unnamed bit-field, which reports `is_nonstatic_data_member()` false.
   * Both come back empty. An annotation is accepted on a template declaration
   * and is readable on a specialization, so the emptiness is what the
   * implementation can describe and not what the declaration carries.
   *
   * Anything else propagates. `annotations_of_with` takes a raw `info`, so a
   * caller can reach `annotations_of` with a value reflection or a
   * `data_member_spec`; both throw on GCC 16.2.1 and neither is a query anyone
   * means to make, so they become a diagnostic instead of a member that
   * silently fails to publish.
   *
   * Catching rather than testing the kind up front ties the answer to
   * `annotations_of`: the day it describes a template, every query built on
   * this one reports the annotations instead of nothing.
   */
  consteval auto annotations_of_or_empty(info R) -> std::vector<info>
  {
    try
    {
      return annotations_of(R);
    }
    catch(std::meta::exception const&)
    {
      if(is_template(R) or is_bit_field(R))
      {
        return {};
      }
      throw;
    }
  }

  consteval auto annotations_of_with(info R, info A)
  {
    return annotations_of_or_empty(R) | std::views::filter([A](auto R) {
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

  /** @brief the value of @p R's annotation of type @p AnnotationType
   *
   * A class type comes back as a `AnnotationType const&`, which costs no copy
   * and works for one that cannot be copied. Anything else comes back by value:
   * an annotation whose type is an enumeration or an arithmetic type is a value
   * and not an object, so there is nothing for a reference to bind to and
   * extract throws *value cannot be extracted*.
   */
  template <typename AnnotationType> consteval decltype(auto) annotation_value_of_with(info R)
  {
    auto annotations = annotations_of_with(R, ^^AnnotationType);
    if(annotations.empty())
    {
      throw std::meta::exception("No such annotation", R);
    }
    else if constexpr(std::is_class_v<AnnotationType> or std::is_union_v<AnnotationType>)
    {
      return extract<AnnotationType const&>(constant_of(annotations.front()));
    }
    else
    {
      return extract<AnnotationType>(constant_of(annotations.front()));
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

  /** @brief how @p R is written at a call site
   *
   * The identifier for an ordinary function, and the spelled operator name for
   * an operator, which carries no identifier of its own. Empty for anything
   * that has neither, a constructor among them.
   *
   * @code
   * spelling_of(^^std::string::size)  // "size"
   * spelling_of(some_operator_plus)   // "operator+"
   * spelling_of(some_operator_new)    // "operator new", the space is required
   * @endcode
   */
  consteval auto spelling_of(info R) -> std::string
  {
    if(has_identifier(R))
    {
      return std::string{identifier_of(R)};
    }
    if(is_operator_function(R) or is_operator_function_template(R))
    {
      const std::string_view symbol = symbol_of(operator_of(R));
      // new, delete and co_await are words, and a word needs separating from
      // the keyword before it.
      const bool word = not symbol.empty() and symbol.front() >= 'a' and symbol.front() <= 'z';
      return std::string{"operator"} + (word ? " " : "") + std::string{symbol};
    }
    return {};
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
    return members_of(R, ctx)                                                //
         | std::views::filter([](info m) { return is_function(m); })         //
         | std::views::filter([name](info m) { return spelling_of(m) == name; }) //
         | std::ranges::to<std::vector<info>>();
  }

  /** @brief every constructor of the class type @p R
   *
   * Constructor templates are absent, for the same reason function templates
   * are absent from functions_named.
   */
  consteval auto constructors_of(info R, access_context ctx = access_context::current())
      -> std::vector<info>
  {
    return members_of(R, ctx)                                        //
         | std::views::filter([](info m) { return is_function(m); }) //
         | std::views::filter(is_constructor)                        //
         | std::ranges::to<std::vector<info>>();
  }

  namespace detail
  {
  template <typename T> consteval auto call_operator() -> meta::info
  {
    return ^^T::operator();
  }

  // Splitting a function type is a pattern match, and partial specialization is
  // the only thing that can do one. Nothing else here is a type list: the
  // parameters are reflections already, so they go into a vector of them, and
  // reflex::constant is what carries a vector through a reflection.
  template <typename> constexpr constant<std::vector<meta::info>> function_parameters{
      std::vector<meta::info>{}};

  template <typename R, typename... A>
  constexpr constant<std::vector<meta::info>> function_parameters<R(A...)>{
      std::vector<meta::info>{^^A...}};
  } // namespace detail

  /** @brief the parameter types of a function type
   *
   * parameters_of does accept a function type, but the parameters it hands back
   * carry no type of their own, so type_of on one throws. The types have to
   * come from taking the type apart instead.
   *
   * return_type_of needs none of this, it reads a function type directly.
   */
  consteval auto function_type_parameters(info R) -> std::vector<info>
  {
    // constant normalizes the vector into a span over static storage, so what
    // comes back out is a view and gets collected here.
    return *extract<constant<std::vector<info>>>(substitute(^^detail::function_parameters, {R})) //
         | std::ranges::to<std::vector<info>>();
  }

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

  /** @brief a member function declared with an explicit object parameter
   *
   * GCC 16 does not ship std::meta::is_explicit_object_member_function, so the
   * query goes through the first parameter, which is what carries the mark.
   */
  consteval auto is_explicit_object_member_function(info R) -> bool
  {
    if(not is_function(R))
    {
      return false;
    }
    const auto params = parameters_of(R);
    return not params.empty() and is_explicit_object_parameter(params.front());
  }

  /** @brief the object parameter of a deducing this member, or meta::null */
  consteval auto explicit_object_type_of(info R) -> info
  {
    return is_explicit_object_member_function(R) ? type_of(parameters_of(R).front()) : meta::null;
  }

  /** @brief a data member holding something callable, like a std::function */
  consteval auto is_invocable_data_member(info R) -> bool
  {
    return is_nonstatic_data_member(R) and callable_function_of(R) != meta::null;
  }

  /** @brief the types of the non-static data members of @p R, in declaration order */
  consteval auto nonstatic_data_member_types_of(
      info R, access_context ctx = access_context::current()) -> std::vector<info>
  {
    return nonstatic_data_members_of(R, ctx)    //
         | std::views::transform(meta::type_of) //
         | std::ranges::to<std::vector<info>>();
  }

  /** @brief everything in @p R that a call to @p name could reach
   *
   * The functions under that name, and failing those, a data member holding a
   * callable, which is reached the same way, `obj.name(args)`. A name is never
   * both, so the two land in one list.
   */
  consteval auto callables_named(
      info R, std::string_view name, access_context ctx = access_context::current())
      -> std::vector<info>
  {
    auto found = functions_named(R, name, ctx);
    if(not found.empty() or not is_type(R))
    {
      return found;
    }
    for(auto m : nonstatic_data_members_of(R, ctx))
    {
      if(has_identifier(m) and identifier_of(m) == name and is_invocable_data_member(m))
      {
        found.push_back(m);
      }
    }
    return found;
  }

  /** @brief the types a call to @p R supplies, without its return type
   *
   * detail::signature_of returns the return type first, which is the wrong
   * shape whenever the parameters are what is being matched.
   *
   * An explicit object parameter is left out: it is the object, not an
   * argument, and no call site writes it.
   */
  consteval auto parameter_types_of(info R) -> std::vector<info>
  {
    if(is_type(R))
    {
      return function_type_parameters(R);
    }
    return parameters_of(R)                                                        //
         | std::views::drop(is_explicit_object_member_function(R) ? 1uz : 0uz)      //
         | std::views::transform(meta::type_of)                                    //
         | std::ranges::to<std::vector<info>>();
  }

  /** @brief the fewest arguments a call to @p R can supply
   *
   * Trailing parameters carrying a default argument may be omitted. A default
   * cannot be followed by a non-defaulted parameter, so walking backwards from
   * the end stops at the first parameter that must be supplied.
   */
  consteval auto min_arity_of(info R) -> std::size_t
  {
    auto params = parameters_of(R);
    // A function type carries no default arguments, and asking one of its
    // parameters throws, so every parameter is required.
    if(is_type(R))
    {
      return params.size();
    }
    std::size_t n = params.size();
    while(n > 0 and has_default_argument(params[n - 1]))
    {
      --n;
    }
    // The explicit object parameter is the object, never an argument.
    return n - (is_explicit_object_member_function(R) ? 1 : 0);
  }

  /** @brief every argument count a call to @p R may supply, shortest first */
  consteval auto arities_of(info R) -> std::vector<std::size_t>
  {
    const std::size_t total =
        parameters_of(R).size() - (is_explicit_object_member_function(R) ? 1 : 0);
    return std::views::iota(min_arity_of(R), total + 1) //
         | std::ranges::to<std::vector<std::size_t>>();
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
