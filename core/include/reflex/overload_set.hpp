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
      return std::meta::return_type_of(function);
    }

    consteval auto parameter_types() const -> std::vector<std::meta::info>
    {
      auto types = is_aggregate_init()
                     ? meta::nonstatic_data_member_types_of(function)
                     : meta::parameter_types_of(function);
      types.resize(arity);
      return types;
    }
  };

  /** @brief every candidate that a call to @p name in @p scope could select
   *
   * @p scope may be a namespace or a class type. An empty @p name on a class
   * type means its constructors, which is how a construction is spelled: there
   * is no identifier to name them with.
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
        constructing ? meta::constructors_of(scope, ctx) : meta::functions_named(scope, name, ctx);

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
      for(auto arity : meta::arities_of(fn))
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

    consteval auto resolver_type(std::meta::info scope, std::string_view name) -> std::meta::info
    {
      if(std::meta::is_namespace(scope))
      {
        return set_type(^^free_candidate, scope, name);
      }
      if(std::meta::is_type(scope) and name.empty())
      {
        return set_type(^^ctor_candidate, scope, name);
      }
      throw std::meta::exception("reflex::resolve: unsupported scope", scope);
    }
  } // namespace detail

  /** @brief the overload set named @p Name in @p Scope, as one callable
   *
   * @code
   * constexpr auto f = reflex::resolve<^^ns, "f">;
   * f(1);                                        // ns::f(int)
   * f(2.0);                                      // ns::f(double)
   * static_assert(not std::invocable<decltype(f), void*>);
   * @endcode
   */
  template <std::meta::info Scope, constant_string Name = "">
  inline constexpr [:detail::resolver_type(Scope, Name.get()):] resolve{};

} // namespace reflex
