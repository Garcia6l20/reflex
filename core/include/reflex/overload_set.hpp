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
    std::meta::info function;
    std::size_t     arity;

    consteval auto return_type() const -> std::meta::info
    {
      return std::meta::return_type_of(function);
    }

    consteval auto parameter_types() const -> std::vector<std::meta::info>
    {
      auto types = meta::parameter_types_of(function);
      types.resize(arity);
      return types;
    }
  };

  /** @brief every candidate that a call to @p name in @p scope could select
   *
   * @p scope may be a namespace or a class type. Function templates are absent:
   * they have no parameter types to match until they are substituted.
   */
  consteval auto overloads_of(
      std::meta::info scope, std::string_view name,
      std::meta::access_context ctx = std::meta::access_context::unchecked())
      -> std::vector<overload>
  {
    std::vector<overload> candidates;
    for(auto fn : meta::functions_named(scope, name, ctx))
    {
      // A defaulted parameter makes one function reachable at several argument
      // counts, and each one is a candidate of its own.
      for(auto arity : meta::arities_of(fn))
      {
        candidates.push_back(overload{fn, arity});
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

    consteval auto candidate_type(std::meta::info tmpl, overload o) -> std::meta::info
    {
      std::vector<std::meta::info> targs{std::meta::reflect_constant(o.function)};
      targs.append_range(o.parameter_types());
      return std::meta::substitute(tmpl, targs);
    }

    consteval auto free_set_type(std::meta::info scope, std::string_view name) -> std::meta::info
    {
      std::vector<std::meta::info> candidates;
      for(auto o : overloads_of(scope, name))
      {
        candidates.push_back(candidate_type(^^free_candidate, o));
      }
      return std::meta::substitute(^^overload_base, candidates);
    }

    consteval auto resolver_type(std::meta::info scope, std::string_view name) -> std::meta::info
    {
      if(std::meta::is_namespace(scope))
      {
        return free_set_type(scope, name);
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
  template <std::meta::info Scope, constant_string Name>
  inline constexpr [:detail::resolver_type(Scope, Name.get()):] resolve{};

} // namespace reflex
