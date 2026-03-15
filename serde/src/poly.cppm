export module reflex.serde:poly;

import :object_visit;

import reflex.core;
import reflex.poly;

import std;

export namespace reflex::serde
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
      return std::forward<Fn>(fn)(it->second);
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
  static inline constexpr decltype(auto) operator()(
      Fn&&             fn,
      std::string_view key,
      Var&&            var) // noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    if(var.is_object())
    {
      return object_visitor<poly::obj<Ts...>>{}(
          std::forward<Fn>(fn), key, std::forward<Var>(var).as_object());
    }
    else
    {
      throw runtime_error("Cannot access key '{}' of non-object value", key);
    }
  }
};

} // namespace reflex::serde
