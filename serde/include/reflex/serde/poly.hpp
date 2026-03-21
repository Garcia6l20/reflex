#pragma once

#include <reflex/exception.hpp>
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
        [&]<typename N>(N&& nested) {
          using U = std::decay_t<N>;
          if constexpr(decays_to_c<N, poly::obj<Ts...>>)
          {
            return object_visitor<poly::obj<Ts...>>{}(
                std::forward<Fn>(fn), key, std::forward<N>(nested));
          }
          else if constexpr(object_visitable_c<U>)
          {
            return object_visit(key, std::forward<N>(nested), std::forward<Fn>(fn));
          }
          else
          {
            throw runtime_error("Cannot access key '{}' of non-object value", key);
          }
        },
        var);
  }
};

} // namespace reflex::serde
