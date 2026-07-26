/** @file
 * @brief naming an overload set and resolving a call against it
 *
 * `^^f` is ill-formed as soon as `f` names more than one function, so an
 * overload set cannot be reflected directly. It is reached through its
 * enclosing scope instead.
 *
 * @code
 * reflex::resolve<^^ns, "f">(1);            // a free function
 * reflex::resolve<^^widget>(8080, "host");  // a construction
 * reflex::resolve<^^widget, "get">(w, 2.0); // a member, object first
 * reflex::resolve<^^widget, "operator+">(a, b);
 *
 * static_assert(std::invocable<decltype(reflex::resolve<^^ns, "f">), int>);
 * static_assert(reflex::overloads_of(^^ns, "f").size() == 3);
 * @endcode
 *
 * Overload resolution is not reimplemented. Every candidate is materialized as
 * a type carrying one concrete operator(), they are inherited into a single
 * type, and the compiler resolves the call. So invocability comes out of
 * std::invocable, the return type out of std::invoke_result_t, and an ambiguous
 * call out of an unsatisfied requirement rather than a hard error.
 *
 * What resolves:
 *  - free functions in a namespace, the global one included
 *  - constructors, and an aggregate's member list, which is a construction path
 *    of its own
 *  - member functions, with const, ref qualifiers and deducing this
 *  - static member functions, with or without an object
 *  - data members holding a callable, reached as `obj.name(args)`
 *  - operators, named the way they are written
 *  - default arguments, as one candidate per reachable arity
 *
 * What does not:
 *  - function templates and templated call operators. `is_function` is false
 *    for a template, and there are no parameter types to match against until it
 *    is substituted. Nothing reports this: such a name simply yields no
 *    candidate, and the call is not invocable.
 *  - expression level operator lookup. Only operators declared in the named
 *    scope are candidates, so built-in operators, free operators reached by
 *    argument dependent lookup from elsewhere, and the candidates rewritten
 *    from a spaceship or an equality are all outside it.
 *  - conversion functions, which have no name to ask for.
 */
#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <meta>
#include <utility>
#include <vector>
#endif

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>

REFLEX_EXPORT namespace reflex
{
  /** @brief one candidate of an overload set
   *
   * A candidate is a function plus the number of arguments it is being
   * considered with, which is not the same as its parameter count once default
   * arguments are in play.
   *
   * Only what is structural is stored, so an overload can cross into a template
   * argument. Everything else is derived on demand.
   */
  struct overload
  {
    /** the function, or the class type itself for an aggregate initialization,
     * which is a construction path with no function behind it */
    std::meta::info function;
    std::size_t     arity;

    consteval auto is_aggregate_init() const -> bool
    {
      return std::meta::is_type(function);
    }

    /** @brief a data member holding a callable, reached as `obj.member(args)` */
    consteval auto is_data_member() const -> bool
    {
      return not is_aggregate_init() and std::meta::is_nonstatic_data_member(function);
    }

    /** @brief the function whose parameters describe this candidate
     *
     * The member itself for a member function, the stored callable for a data
     * member, and null for an aggregate initialization, which has a member list
     * instead of a parameter list.
     */
    consteval auto signature() const -> std::meta::info
    {
      if(is_aggregate_init())
      {
        return std::meta::info{};
      }
      return is_data_member() ? meta::callable_function_of(function) : function;
    }

    consteval auto return_type() const -> std::meta::info
    {
      if(is_aggregate_init())
      {
        return function;
      }
      if(std::meta::is_constructor(function))
      {
        return std::meta::parent_of(function);
      }
      const auto sig = signature();
      // A function type has no return_type_of, only a decomposition.
      return std::meta::is_type(sig) ? meta::function_type_parts(sig).front()
                                     : std::meta::return_type_of(sig);
    }

    /** @brief what this candidate could take, before the arity cuts it down */
    consteval auto all_parameter_types() const -> std::vector<std::meta::info>
    {
      return is_aggregate_init() ? meta::nonstatic_data_member_types_of(function)
                                 : meta::parameter_types_of(signature());
    }

    consteval auto parameter_types() const -> std::vector<std::meta::info>
    {
      auto types = all_parameter_types();
      types.resize(arity);
      return types;
    }
  };

  /** @brief every candidate that a call to @p name in @p scope could select
   *
   * @p scope may be a namespace or a class type. An empty @p name on a class
   * type means its constructors, which is how a construction is spelled: there
   * is no identifier to name them with, and an aggregate's member list joins
   * them as a construction path of its own.
   *
   * An operator is named the way it is written, `"operator+"`. A data member
   * holding a callable is a candidate too, since it is reached the same way a
   * member function is.
   *
   * Function templates are absent: they have no parameter types to match until
   * they are substituted.
   */
  consteval auto overloads_of(
      std::meta::info scope, std::string_view name,
      std::meta::access_context ctx = std::meta::access_context::unchecked())
      -> std::vector<overload>
  {
    const bool constructing = name.empty();
    const bool aggregate    = constructing and std::meta::is_aggregate_type(scope);

    const auto functions =
        constructing ? meta::constructors_of(scope, ctx) : meta::callables_named(scope, name, ctx);

    std::vector<overload> candidates;
    for(auto fn : functions)
    {
      // An aggregate has no default constructor to speak of: the member list
      // covers the same call, and keeping both would make it ambiguous.
      if(aggregate and std::meta::is_default_constructor(fn))
      {
        continue;
      }
      // A defaulted parameter makes one function reachable at several argument
      // counts, and each one is a candidate of its own.
      for(auto arity : meta::arities_of(overload{fn, 0}.signature()))
      {
        candidates.push_back(overload{fn, arity});
      }
    }

    if(aggregate)
    {
      // Parenthesized aggregate initialization takes a prefix of the member
      // list. A member cannot be skipped, only dropped from the end, so the
      // reachable argument counts run from none to all of them.
      const auto members = std::meta::nonstatic_data_members_of(scope, ctx).size();
      for(std::size_t arity = 0; arity <= members; ++arity)
      {
        candidates.push_back(overload{scope, arity});
      }
    }
    return candidates;
  }

  namespace detail
  {
    /** @brief the resolution engine
     *
     * Overload resolution is not reimplemented here. Each candidate becomes a
     * type with one concrete operator(), they are all inherited into a single
     * type, and the compiler resolves the call. Invocability then falls out of
     * std::invocable, the return type out of std::invoke_result_t, and an
     * ambiguous call out of an unsatisfied requirement rather than an error.
     */
    template <typename... Cs> struct overload_base : Cs...
    {
      using Cs::operator()...;
    };

    template <std::meta::info Fn, typename... Args> struct free_candidate
    {
      constexpr decltype(auto) operator()(Args... args) const
      {
        return [:Fn:](std::forward<Args>(args)...);
      }
    };

    // T is named rather than the constructor: a constructor cannot be called
    // through its own reflection, only through the type it builds.
    template <typename T, typename... Args> struct ctor_candidate
    {
      constexpr auto operator()(Args... args) const -> T
      {
        return T(std::forward<Args>(args)...);
      }
    };

    /** @brief a candidate that cannot be called
     *
     * A deleted function still takes part in overload resolution, and selecting
     * it is what makes the call ill-formed. Dropping it from the set instead
     * would silently hand the call to a worse candidate, so it is materialized
     * as deleted and the compiler reaches the same verdict as it would on the
     * real function.
     */
    template <std::meta::info Fn, typename... Args> struct deleted_candidate
    {
      constexpr void operator()(Args...) const = delete;
    };

    consteval auto candidate_type(std::meta::info tmpl, overload o) -> std::meta::info
    {
      const bool aggregate = o.is_aggregate_init();
      const bool deleted   = not aggregate and std::meta::is_deleted(o.function);
      const bool builds    = aggregate or std::meta::is_constructor(o.function);

      // A construction names the type it builds, anything else names its
      // function, since a constructor cannot be called through its reflection.
      std::vector<std::meta::info> targs{
          builds and not deleted ? o.return_type() : std::meta::reflect_constant(o.function)};
      targs.append_range(o.parameter_types());
      return std::meta::substitute(deleted ? ^^deleted_candidate : tmpl, targs);
    }

    consteval auto set_type(
        std::meta::info tmpl, std::meta::info scope, std::string_view name) -> std::meta::info
    {
      std::vector<std::meta::info> candidates;
      for(auto o : overloads_of(scope, name))
      {
        candidates.push_back(candidate_type(tmpl, o));
      }
      return std::meta::substitute(^^overload_base, candidates);
    }

    /** @brief a member function candidate, with its object already chosen
     *
     * The object is held rather than passed, so operator() carries the
     * arguments alone. Passing it as an ordinary first parameter would let the
     * object conversion compete with the argument conversions, which is not how
     * the implicit object parameter is ranked: `w.convert(2.0)` against an
     * `convert(int)` and a `convert(double) const` is unambiguous, while the
     * same pair written as two-parameter functions is not.
     */
    template <std::meta::info Fn, typename Self, typename... Args> struct member_candidate
    {
      Self self;

      constexpr decltype(auto) operator()(Args... args) const
      {
        return (std::forward<Self>(self).[:Fn:])(std::forward<Args>(args)...);
      }
    };

    template <std::meta::info Fn, typename Self, typename... Args>
    struct deleted_member_candidate
    {
      Self self;

      constexpr void operator()(Args...) const = delete;
    };

    template <typename Self, typename... Cs> struct bound_overload_base : Cs...
    {
      using Cs::operator()...;

      constexpr explicit bound_overload_base(Self s) : Cs{static_cast<Self>(s)}...
      {}
    };

    /** @brief can @p m be called on an object of type @p self
     *
     * @p self carries both constness and value category, as a reference type.
     * This is the viability half of what the language does with the implicit
     * object parameter before it ranks anything.
     */
    consteval auto callable_on(std::meta::info m, std::meta::info self) -> bool
    {
      // A static member has no object parameter to satisfy, and naming it
      // through an object is legal whatever that object is qualified with. A
      // data member is reached the same way, and its own constness is the
      // object's, which the candidate body settles rather than this.
      if(std::meta::is_static_member(m) or std::meta::is_nonstatic_data_member(m))
      {
        return true;
      }

      const bool self_const  = std::meta::is_const_type(std::meta::remove_reference(self));
      const bool self_rvalue = std::meta::is_rvalue_reference_type(self);

      // A deducing this member spells its object out as an ordinary parameter,
      // so what it accepts is read off that parameter rather than off the
      // qualifiers, which it carries none of.
      const auto object = meta::explicit_object_type_of(m);
      if(object != meta::null)
      {
        if(not std::meta::is_reference_type(object))
        {
          return true; // taken by value, so any object will do
        }
        if(self_const and not std::meta::is_const_type(std::meta::remove_reference(object)))
        {
          return false;
        }
        return self_rvalue == std::meta::is_rvalue_reference_type(object)
            or std::meta::is_const_type(std::meta::remove_reference(object));
      }

      if(self_const and not std::meta::is_const(m))
      {
        return false;
      }
      if(self_rvalue and std::meta::is_lvalue_reference_qualified(m))
      {
        return false;
      }
      if(not self_rvalue and std::meta::is_rvalue_reference_qualified(m))
      {
        return false;
      }
      return true;
    }

    /** @brief how closely @p m wants the object it is called on
     *
     * Used only to break a tie between two members with the same parameter
     * list, which the language resolves on the object parameter alone.
     */
    consteval auto self_rank(std::meta::info m) -> int
    {
      int rank = std::meta::is_const(m) ? 0 : 2;
      if(std::meta::is_lvalue_reference_qualified(m) or std::meta::is_rvalue_reference_qualified(m))
      {
        rank += 1;
      }
      return rank;
    }

    consteval auto bound_set_type(
        std::meta::info scope, std::meta::info self, std::string_view name) -> std::meta::info
    {
      std::vector<std::meta::info> chosen;
      for(auto m : meta::callables_named(scope, name, std::meta::access_context::unchecked()))
      {
        if(not callable_on(m, self))
        {
          continue;
        }
        // Overloads differing only in their qualifiers are one name to the
        // caller, and the object decides between them. Keeping both would leave
        // two identical operator() to choose from and make every call ambiguous.
        const auto params    = overload{m, 0}.all_parameter_types();
        bool       displaced = false;
        for(auto& c : chosen)
        {
          if(overload{c, 0}.all_parameter_types() == params)
          {
            if(self_rank(m) > self_rank(c))
            {
              c = m;
            }
            displaced = true;
            break;
          }
        }
        if(not displaced)
        {
          chosen.push_back(m);
        }
      }

      std::vector<std::meta::info> candidates{self};
      for(auto m : chosen)
      {
        const bool deleted =
            not std::meta::is_nonstatic_data_member(m) and std::meta::is_deleted(m);
        const auto tmpl = deleted ? ^^deleted_member_candidate : ^^member_candidate;
        for(auto arity : meta::arities_of(overload{m, 0}.signature()))
        {
          std::vector<std::meta::info> targs{std::meta::reflect_constant(m), self};
          targs.append_range(overload{m, arity}.parameter_types());
          candidates.push_back(std::meta::substitute(tmpl, targs));
        }
      }
      return std::meta::substitute(^^bound_overload_base, candidates);
    }

    /** @brief the members of @p scope named @p name that need no object */
    consteval auto static_set_type(std::meta::info scope, std::string_view name) -> std::meta::info
    {
      std::vector<std::meta::info> candidates;
      for(auto o : overloads_of(scope, name))
      {
        if(std::meta::is_static_member(o.function))
        {
          candidates.push_back(candidate_type(^^free_candidate, o));
        }
      }
      return std::meta::substitute(^^overload_base, candidates);
    }

    template <std::meta::info Scope, constant_string Name, typename Self>
    using bound_set_t = [:bound_set_type(Scope, ^^Self&&, Name.get()):];

    template <std::meta::info Scope, constant_string Name>
    using static_set_t = [:static_set_type(Scope, Name.get()):];

    // These requirements name a variable rather than the alias directly: a
    // requires-clause mentioning a consteval splice alias corrupts the splice
    // on GCC 16.
    //
    // The object has to be one, not merely the first argument. Without that,
    // a static call whose first argument happens to fit would be taken for a
    // member call and fail inside the candidate rather than here.
    template <std::meta::info Scope> using scope_t = [:Scope:];

    // The object test has to be settled before the set is built at all, hence
    // if constexpr rather than a conjunction: a set bound to something that is
    // not an object of the class still substitutes, and asking whether it is
    // invocable then instantiates a candidate body that cannot compile.
    template <std::meta::info Scope, constant_string Name, typename Self, typename... Args>
    consteval auto bound_invocable_of() -> bool
    {
      if constexpr(std::derived_from<std::remove_cvref_t<Self>, scope_t<Scope>>)
      {
        return std::invocable<bound_set_t<Scope, Name, Self>, Args...>;
      }
      else
      {
        return false;
      }
    }

    template <std::meta::info Scope, constant_string Name, typename Self, typename... Args>
    constexpr bool bound_invocable = bound_invocable_of<Scope, Name, Self, Args...>();

    template <std::meta::info Scope, constant_string Name, typename... Args>
    constexpr bool static_invocable = std::invocable<static_set_t<Scope, Name>, Args...>;

    /** @brief resolves a member call once the object is known
     *
     * Self is deduced rather than fixed, so it takes no part in ranking the
     * arguments. The set is built for that exact object type, which is what
     * makes const, ref qualified and plain overloads land the way they do on a
     * direct call.
     */
    template <std::meta::info Scope, constant_string Name> struct member_resolver
    {
      template <typename Self, typename... Args>
        requires bound_invocable<Scope, Name, Self, Args...>
      constexpr decltype(auto) operator()(Self&& self, Args&&... args) const
      {
        using set_type = [:bound_set_type(Scope, ^^Self&&, Name.get()):];
        return set_type{std::forward<Self>(self)}(std::forward<Args>(args)...);
      }

      // A static member needs no object, so it is reachable with the arguments
      // alone. It stays in the bound set as well, since a static member can be
      // named through an object too.
      template <typename... Args>
        requires static_invocable<Scope, Name, Args...>
      constexpr decltype(auto) operator()(Args&&... args) const
      {
        using set_type = [:static_set_type(Scope, Name.get()):];
        return set_type{}(std::forward<Args>(args)...);
      }
    };

    template <std::meta::info Scope, constant_string Name>
    consteval auto resolver_type() -> std::meta::info
    {
      const std::string_view name = Name.get();
      if(std::meta::is_namespace(Scope))
      {
        return set_type(^^free_candidate, Scope, name);
      }
      if(std::meta::is_type(Scope) and name.empty())
      {
        return set_type(^^ctor_candidate, Scope, name);
      }
      if(std::meta::is_type(Scope))
      {
        return ^^member_resolver<Scope, Name>;
      }
      throw std::meta::exception("reflex::resolve: unsupported scope", Scope);
    }
  } // namespace detail

  /** @brief the overload set named @p Name in @p Scope, as one callable
   *
   * @code
   * constexpr auto f = reflex::resolve<^^ns, "f">;
   * f(1);                                        // ns::f(int)
   * f(2.0);                                      // ns::f(double)
   * static_assert(not std::invocable<decltype(f), void*>);
   *
   * reflex::resolve<^^widget>(1, "x");           // a construction
   * reflex::resolve<^^widget, "convert">(w, 2.0) // a member, object first
   * @endcode
   */
  template <std::meta::info Scope, constant_string Name = "">
  inline constexpr [:detail::resolver_type<Scope, Name>():] resolve{};

} // namespace reflex
