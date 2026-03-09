export module reflex.core:visit;

import :meta;
import std;

#define fwd(...) std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

namespace reflex::detail
{
constexpr auto __visitor_auto_deduction_helper = []<typename T>(T&&) {};

struct any_visitor
{
  inline void operator()(auto&&)
  {
  }
};
} // namespace reflex::detail

export namespace reflex
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
      operator()(Fn&& fn, VarT&& v) noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Ts)))
    {
      if(v.index() == ii)
      {
        return fwd(fn)(std::get<ii>(fwd(v)));
      }
    }
    std::unreachable();
  }
};

/** @brief member-visitable visitor */
template <template <typename...> typename Tmpl, typename... Ts>
  requires requires(Tmpl<Ts...> o) { o.visit(detail::__visitor_auto_deduction_helper); }
struct visitor<Tmpl<Ts...>>
{
  template <typename Fn, typename VarT>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, VarT&& v) noexcept(noexcept(fwd(v).visit(fwd(fn))))
  {
    return fwd(v).visit(fwd(fn));
  }
};

template <typename T>
concept visitable_c = requires(T&& v) {
  {
    visitor<std::decay_t<T>>::operator()([](auto&&) {}, fwd(v))
  };
};

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
  if constexpr(visitable_c<Head>)
  {
    return visitor<std::decay_t<Head>>::operator()(
        [&](auto&& val) -> decltype(auto) { //
          if constexpr(sizeof...(Tail) == 0)
          {
            return fwd(fn)(fwd(val));
          } else {
            return visit([&](auto &&...rest) -> decltype(auto) {
              return fwd(fn)(fwd(val), fwd(rest)...);
            }, fwd(tail)...);
          }
        },
        fwd(head));
  }
  else
  {
    if constexpr(sizeof...(Tail) == 0)
    {
      return fwd(fn)(fwd(head));
    }
    else
    {
      return visit( //
          [&](auto&&... rest) -> decltype(auto) { return fwd(fn)(fwd(head), fwd(rest)...); },
          fwd(tail)...);
    }
  }
}
} // namespace reflex
