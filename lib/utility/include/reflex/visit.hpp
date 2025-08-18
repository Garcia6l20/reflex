#pragma once

#include <reflex/fwd.hpp>
#include <reflex/match.hpp>
#include <reflex/meta.hpp>

#include <variant>

namespace reflex
{
consteval bool is_variant_type(meta::info R)
{
  const auto RR = decay(R);
  return has_template_arguments(RR) and (template_of(RR) == ^^std::variant);
}

template <typename T>
concept variant_c = is_variant_type(^^T);

template <typename T>
concept non_variant_c = not variant_c<T>;

template <typename... Ts> struct visitor;

/** @brief variant visitor */
template <typename... Ts> struct visitor<std::variant<Ts...>>
{
  template <typename Fn, typename VarT>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, VarT&& v) noexcept(noexcept(reflex_fwd(fn)(std::get<0>(reflex_fwd(v)))))
  {
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Ts)))
    {
      if(v.index() == ii)
      {
        return reflex_fwd(fn)(std::get<ii>(reflex_fwd(v)));
      }
    }
    std::unreachable();
  }
};

/** @brief member-visitable visitor */
template <template <typename...> typename Tmpl, typename... Ts>
  requires requires(Tmpl<Ts...> o) { o.visit([](auto&&...) {}); }
struct visitor<Tmpl<Ts...>>
{
  template <typename Fn, typename VarT>
  static inline constexpr decltype(auto) operator()(Fn&&   fn,
                                                    VarT&& v) noexcept(noexcept(reflex_fwd(v).visit(reflex_fwd(fn))))
  {
    return reflex_fwd(v).visit(reflex_fwd(fn));
  }
};

namespace detail
{
struct any_visitor
{
  inline void operator()(auto&&) {}
};
} // namespace detail

consteval bool is_visitable_type(meta::info t)
{
  auto type = decay(t);
  if(can_substitute(^^visitor,
                    {
                        type}))
  {
    return is_invocable_type(substitute(^^visitor,
                                        {
                                            type}),
                             {^^detail::any_visitor, type});
  }
  return false;
}

template <typename Fn, typename Head, typename... Tail>
inline constexpr decltype(auto) visit(Fn&& fn, Head&& head, Tail&&... tail)
{
  if constexpr(is_visitable_type(^^Head))
  {
    return visitor<std::decay_t<Head>>::operator()(
        [&](auto&& val) -> decltype(auto) { //
          if constexpr(sizeof...(Tail) == 0)
          {
            return reflex_fwd(fn)(reflex_fwd(val));
          } else {
            return visit([&](auto &&...rest) -> decltype(auto) {
              return reflex_fwd(fn)(reflex_fwd(val), reflex_fwd(rest)...);
            }, reflex_fwd(tail)...);
          }
        },
        reflex_fwd(head));
  }
  else
  {
    if constexpr(sizeof...(Tail) == 0)
    {
      return reflex_fwd(fn)(reflex_fwd(head));
    }
    else
    {
      return visit( //
          [&](auto&&... rest) -> decltype(auto) { return reflex_fwd(fn)(reflex_fwd(head), reflex_fwd(rest)...); },
          reflex_fwd(tail)...);
    }
  }
}
} // namespace reflex
