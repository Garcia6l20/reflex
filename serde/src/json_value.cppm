export module reflex.serde.json:value;

import reflex.serde;
import reflex.poly;

import std;

export namespace reflex::serde::json
{
using null_t = reflex::poly::null_t;

constexpr null_t null{};

using string  = std::string;
using number  = double;
using boolean = bool;

using value  = poly::var<number, boolean, string>;
using object = value::obj_type;
using array  = value::arr_type;

} // namespace reflex::serde::json

export namespace reflex::serde
{
template <> struct object_visitor<json::object>
{
  template <typename Fn, typename StrT, decays_to_c<json::object> Obj>
  static inline constexpr decltype(auto)
      operator()(Fn&& fn, StrT&& key, Obj&& obj) // noexcept(noexcept(fwd(fn)(std::get<0>(fwd(v)))))
  {
    if(obj.contains(key))
    {
      return std::forward<Fn>(fn)(std::forward<Obj>(obj)[std::forward<StrT>(key)]);
    }
    else
    {
      return std::forward<Fn>(fn)(std::forward<Obj>(obj)
                                      .emplace(std::forward<decltype(key)>(key), json::null)
                                      .first->second);
    }
  }
};
} // namespace reflex::serde
