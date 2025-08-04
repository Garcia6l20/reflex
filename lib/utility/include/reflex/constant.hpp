#pragma once

#include <reflex/meta.hpp>

namespace reflex
{
namespace meta
{
template <typename T> constexpr auto define_static_object(T const& obj)
{
  return std::define_static_array(std::span(std::addressof(obj), 1))[0];
}

namespace detail
{
template <typename T> struct constant_wrapper;

template <typename T> using constant_value_t = typename constant_wrapper<T>::value_type;

template <class T>
using constant_value_or_ref_t = [:is_lvalue_reference_type(^^T) ? ^^T
                                                                : substitute(^^constant_value_t,
                                                                             {
                                                                                 ^^T}):];

template <typename T>
  requires(is_structural_type(^^T))
struct constant_wrapper<T>
{
  using value_type = T;

  constexpr static value_type wrap(value_type v)
  {
    return v;
  }
};

namespace representation_object
{
template <class T, std::meta::info... Is> inline constexpr constant_value_t<T> array[] = {[:Is:]...};
template <class T, std::meta::info... Is>
inline constexpr constant_value_t<T> object = []
{
  if constexpr(requires { constant_wrapper<T>::unwrap(Is...); })
  {
    return constant_wrapper<T>::unwrap(Is...);
  }
  else if constexpr(sizeof...(Is) == 1)
  {
    return extract<constant_value_t<T>>(Is...[0]);
  }
  else
  {
    std::unreachable();
  }
}();
} // namespace representation_object

template <typename T, typename... Infos> consteval decltype(auto) make_object(Infos... R)
{
  return extract<T const&>(object_of(substitute(^^representation_object::object,
                                                {
                                                    ^^T,
                                                    R...})));
}

template <> struct constant_wrapper<std::string_view>
{
  using value_type = std::string_view;
  static consteval decltype(auto) wrap(std::string_view const& s)
  {
    return make_object<value_type>(std::meta::reflect_constant(std::meta::reflect_constant_string(s)));
  }
  static consteval value_type unwrap(std::meta::info r)
  {
    return value_type(extract<char const*>(r), extent(type_of(r)) - 1);
  }
};
template <> struct constant_wrapper<const char*> : constant_wrapper<std::string_view>
{
};
template <size_t N> struct constant_wrapper<const char[N]> : constant_wrapper<std::string_view>
{
};
template <> struct constant_wrapper<std::string> : constant_wrapper<std::string_view>
{
};

template <std::ranges::input_range R> inline constexpr auto reflect_constant_array(R&& r)
{
  std::vector<std::meta::info> elems = {^^std::ranges::range_value_t<R>};
  for(auto&& e : r)
  {
    elems.push_back(reflect_constant(reflect_constant(e)));
  }
  return substitute(^^representation_object::array, elems);
};

template <std::ranges::input_range R>
  requires(not is_structural_type(^^R)) // use direct representation for structural ranges (ie.: std::array)
struct constant_wrapper<R>
{
  using element_type = std::ranges::range_value_t<R>;
  using value_type   = std::span<std::add_const_t<element_type>>;

  static consteval value_type const& wrap(R const& v)
  {
    return make_object<value_type>(std::meta::reflect_constant(reflect_constant_array(v)));
  }

  static consteval value_type unwrap(std::meta::info r)
  {
    return value_type(extract<element_type const*>(r), extent(type_of(r)));
  }
};

template <typename T> consteval decltype(auto) object_reflection(T&& obj)
{
  if constexpr(requires { reflect_constant(obj); })
  {
    return reflect_constant(reflect_constant(obj));
  }
  else
  {
    return reflect_constant(object_of(reflect_object(obj)));
  }
}

template <typename... Ts> struct constant_wrapper<std::tuple<Ts...>>
{
  using value_type = std::tuple<constant_value_or_ref_t<Ts>...>;
  static consteval value_type const& wrap(std::tuple<Ts...> const& v)
  {
    return std::apply(
        [&](Ts const&... items) -> value_type const&
        { return make_object<value_type>(object_reflection(constant_wrapper<std::decay_t<Ts>>::wrap(items))...); },
        v);
  }
  template <typename... Infos> static consteval value_type unwrap(Infos... r)
  {
    return value_type(extract<constant_value_or_ref_t<Ts>>(r)...);
  }
};

} // namespace detail

template <typename T> struct constant
{
  using value_type = detail::constant_value_t<T>;

  value_type const& value;

  consteval constant(T const& v) : value{detail::constant_wrapper<T>::wrap(v)}
  {
  }

  template <typename U>
  consteval constant(U const& v)
    requires requires { detail::constant_wrapper<T>::wrap(v); }
      : value{detail::constant_wrapper<T>::wrap(v)}
  {
  }

  consteval value_type const& get() const
  {
    return value;
  }
  consteval operator value_type const&() const
  {
    return value;
  }
  consteval value_type const& operator*() const
  {
    return value;
  }
  consteval value_type const* operator->() const
  {
    return std::addressof(value);
  }
};

// avoid extra overloading for string types, forwarded them to constant<std::string_view>
template <size_t N> constant(const char (&s)[N]) -> constant<std::string_view>;
constant(std::string const& s) -> constant<std::string_view>;

template <typename T>
  requires(is_structural_type(^^T))
struct constant<T>
{
  using value_type = T;
  value_type value;

  consteval constant(T const& v) : value{v}
  {
  }

  consteval value_type const& get() const
  {
    return value;
  }
  consteval operator value_type const&() const
  {
    return value;
  }
  consteval value_type const& operator*() const
  {
    return value;
  }
  consteval value_type const* operator->() const
  {
    return std::addressof(value);
  }
};

} // namespace meta

template <typename T> using constant = meta::constant<T>;

using constant_string = meta::constant<std::string_view>;

} // namespace reflex
