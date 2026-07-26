
#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <type_traits>
#include <utility>
#endif

#include <reflex/concepts.hpp>

REFLEX_EXPORT namespace reflex
{
  namespace _tag_invoke
  {
  struct _fn
  {
    template <typename Cpo, typename... Args>
    [[gnu::always_inline]] inline constexpr auto operator()(Cpo cpo, Args&&... args) const
        noexcept(noexcept(tag_invoke(static_cast<Cpo&&>(cpo), static_cast<Args&&>(args)...)))
            -> decltype(tag_invoke(static_cast<Cpo&&>(cpo), static_cast<Args&&>(args)...))
    {
      return tag_invoke(static_cast<Cpo&&>(cpo), static_cast<Args&&>(args)...);
    }
  };

  template <typename Cpo, typename... Args>
  using tag_invoke_result_t =
      decltype(tag_invoke(std::declval<Cpo&&>(), std::declval<Args&&>()...));
  } // namespace _tag_invoke

  namespace _tag_invoke_cpo
  {
  inline constexpr _tag_invoke::_fn tag_invoke{};
  }
  using namespace _tag_invoke_cpo;

  // Library code inside namespace reflex must call the object through this
  // name, never as a bare `tag_invoke(...)`. Unqualified lookup from inside
  // reflex finds both the object above, which the using-directive makes a
  // member of reflex, and every tag_invoke overload any header has already
  // declared there. A variable and a function set under one name is ambiguous,
  // so a bare call compiles or not depending on which headers were included
  // first. Reaching the object by its own namespace has neither problem, and
  // it still dispatches through ADL exactly as before.
  //
  // Header-only builds are what this protects: a module build never sees the
  // headers in a user-chosen order.

  using _tag_invoke::tag_invoke_result_t;

  template <auto const& Cpo> using tag_t = std::remove_cvref_t<decltype(Cpo)>;

  template <typename Cpo, typename... Args>
  concept tag_invocable_c = requires(Cpo cpo, Args&&... args) {
    { tag_invoke(cpo, std::forward<Args>(args)...) };
  };

  template <typename Cpo, typename... Args>
  concept nothrow_tag_invocable_c = requires(Cpo cpo, Args&&... args) {
    { tag_invoke(cpo, std::forward<Args>(args)...) } noexcept;
  };

  /// Default-layer tag: library-provided implementations of a CPO register
  /// against cpo_default<Cpo> instead of Cpo, so user overloads always win.
  template <typename Cpo> struct cpo_default
  {
  };

  /// Mirrors tag_t: tag_default_t<reflex::hash> names the default-layer tag type.
  template <auto const& Cpo>
  using tag_default_t = cpo_default<std::remove_cvref_t<decltype(Cpo)>>;

  template <typename Cpo, typename... Args>
  concept tag_default_invocable_c = tag_invocable_c<cpo_default<Cpo>, Args...>;

  template <typename Cpo, typename... Args>
  concept nothrow_tag_default_invocable_c = nothrow_tag_invocable_c<cpo_default<Cpo>, Args...>;

  /// Either layer viable.
  template <typename Cpo, typename... Args>
  concept customizable_c =
      tag_invocable_c<Cpo, Args...> or tag_default_invocable_c<Cpo, Args...>;

  struct customization_point_object
  {
    // user layer: any user tag_invoke overload, exact-type or concept-based
    template <typename Self, typename... Args>
      requires(tag_invocable_c<Self, Args && ...>)
    [[gnu::always_inline]] inline constexpr decltype(auto) operator()(
        this Self const& self,
        Args&&... args) noexcept(nothrow_tag_invocable_c<Self, Args&&...>)
    {
      return tag_invoke(self, std::forward<Args>(args)...);
    }

    // default layer: consulted only when no user overload is viable
    template <typename Self, typename... Args>
      requires(not tag_invocable_c<Self, Args && ...>)
              and tag_default_invocable_c<Self, Args&&...>
    [[gnu::always_inline]] inline constexpr decltype(auto) operator()(
        this Self const&,
        Args&&... args) noexcept(nothrow_tag_default_invocable_c<Self, Args&&...>)
    {
      return tag_invoke(cpo_default<Self>{}, std::forward<Args>(args)...);
    }
  };
} // namespace reflex