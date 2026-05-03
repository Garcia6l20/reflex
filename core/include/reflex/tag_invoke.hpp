
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

  struct customization_point_object
  {
    template <typename Self, typename... Args>
      requires(tag_invocable_c<Self, Args && ...>)
    [[gnu::always_inline]] inline constexpr decltype(auto) operator()(
        this Self const& self,
        Args&&... args) noexcept(nothrow_tag_invocable_c<Self, Args&&...>)
    {
      return tag_invoke(self, std::forward<Args>(args)...);
    }
  };
} // namespace reflex