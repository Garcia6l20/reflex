#pragma once

#include <reflex/poly.hpp>

#include <reflex/serde/object_visit.hpp>

namespace reflex::serde
{

template <typename... Ts> struct object_visitor<poly::obj<Ts...>>
{
  template <typename Fn, decays_to_c<poly::obj<Ts...>> Obj>
  static inline constexpr decltype(auto) operator()(
      Fn&&             fn,
      std::string_view key,
      Obj&&            obj) // noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    auto it = std::ranges::find_if(obj, [key](auto const& elem) { return elem.first == key; });
    if(it != obj.end())
    {
      return reflex::visit(std::forward<Fn>(fn), it->second);
    }
    else
    {
      if constexpr(not std::is_const_v<std::remove_reference_t<decltype(obj)>>)
      {
        return std::forward<Fn>(fn)(
            std::forward<Obj>(obj).emplace(std::string{key}, poly::null).first->second);
      }
    }
  }
};

template <typename... Ts> struct object_visitor<poly::var<Ts...>>
{
  template <typename Fn, decays_to_c<poly::var<Ts...>> Var>
  static inline constexpr decltype(auto) operator()(Fn&& fn, std::string_view key, Var&& var)
  {
    using return_type = decltype(std::forward<Fn>(fn)(std::declval<poly::null_t&&>()));
    return visit(
        match{
            // clang-format off
            [&]<decays_to_c<poly::obj<Ts...>> T>(T&& obj) -> return_type {
              return object_visitor<poly::obj<Ts...>>{}(
                  std::forward<Fn>(fn), key, std::forward<T>(obj));
            },
            [&]<object_visitable_c T>(T&& v) -> return_type
              requires(not decays_to_c<T, poly::obj<Ts...>>)
            { return object_visit(key, std::forward<T>(v), std::forward<Fn>(fn)); },
            [&]<typename T>(T&& obj) -> return_type
              requires(not decays_to_c<T, poly::obj<Ts...>> and not object_visitable_c<T>)
            { throw runtime_error("Cannot access key '{}' of non-object value", key); }},
            // clang-format on
            var);
  }
};

} // namespace reflex::serde
