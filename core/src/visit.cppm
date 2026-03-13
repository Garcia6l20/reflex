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
  {}
};
} // namespace reflex::detail

export namespace reflex
{
consteval std::meta::info variant_type_of(meta::info R)
{
  auto const RR = dealias(decay(R));
  if(not is_class_type(RR))
  {
    throw std::meta::exception("not a class type", R);
  }
  if(has_template_arguments(RR) and (template_of(RR) == ^^std::variant))
  {
    return RR;
  }
  auto bases = bases_of(RR, std::meta::access_context::current());
  auto base  = std::ranges::find_if(bases, [](meta::info base) {
    auto T = dealias(type_of(base));
    return has_template_arguments(T) and (template_of(T) == ^^std::variant);
  });
  if(base == std::ranges::end(bases))
  {
    throw std::meta::exception("not a variant type", R);
  }
  return dealias(type_of(*base));
}

consteval bool is_variant_type(meta::info R)
{
  try
  {
    variant_type_of(R);
    return true;
  }
  catch(std::meta::exception const&)
  {
    return false;
  }
}

template <typename T>
concept variant_c = is_variant_type(^^T);

template <typename T>
concept non_variant_c = not variant_c<T>;

template <typename...> struct visitor;

/** @brief variant visitor */
template <variant_c T> struct visitor<T>
{
  template <typename Fn, typename VarT>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, VarT&& v) noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    static constexpr auto value_types =
        define_static_array(template_arguments_of(variant_type_of(^^T)));
    template for(constexpr auto ii : std::views::iota(0uz, value_types.size()))
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
template <non_variant_c T>
  requires requires(T o) { o.visit(detail::any_visitor{}); }
struct visitor<T>
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
  if(can_substitute(
         ^^visitor, {
                        type}))
  {
    return is_invocable_type(
        substitute(
            ^^visitor,
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
          }
          else
          {
            return visit(
                [&](auto&&... rest) -> decltype(auto) { return fwd(fn)(fwd(val), fwd(rest)...); },
                fwd(tail)...);
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
