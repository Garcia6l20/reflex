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
  else if constexpr(requires { constant_wrapper<T>::template unwrap<Is...>(); })
  {
    return constant_wrapper<T>::template unwrap<Is...>();
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
template <size_t N> struct constant_wrapper<char[N]> : constant_wrapper<std::string_view>
{
};
template <> struct constant_wrapper<std::string> : constant_wrapper<std::string_view>
{
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

template <std::ranges::input_range R> inline constexpr auto reflect_constant_array(R&& r)
{
  using wrapper                      = constant_wrapper<std::ranges::range_value_t<R>>;
  std::vector<std::meta::info> elems = {^^std::ranges::range_value_t<R>};
  for(auto&& e : r)
  {
    elems.push_back(object_reflection(wrapper::wrap(e)));
  }
  return substitute(^^representation_object::array, elems);
};

template <std::ranges::input_range R>
  requires(not is_structural_type(^^R)) // use direct representation for structural ranges (ie.: std::array)
struct constant_wrapper<R>
{
  using element_type = std::add_const_t<constant_value_or_ref_t<std::ranges::range_value_t<R>>>;
  using value_type   = std::span<element_type>;

  template <std::ranges::input_range R2>
    requires(std::constructible_from<element_type, std::ranges::range_value_t<R2>>)
  static consteval value_type const& wrap(R2 const& v)
  {
    return make_object<value_type>(std::meta::reflect_constant(reflect_constant_array(v)));
  }

  // construct range from single value
  template <typename T>
    requires(std::constructible_from<element_type, T>)
  static consteval value_type const& wrap(T const& v)
  {
    return wrap(R{v});
  }

  template <typename... Ts>
    requires(std::constructible_from<element_type, Ts> and ...)
  static consteval value_type const& wrap(Ts const&... v)
  {
    return wrap(R{v...});
  }

  static consteval value_type unwrap(std::meta::info r)
  {
    return value_type(extract<element_type const*>(r), extent(type_of(r)));
  }
};

template <typename... Ts> struct constant_wrapper<std::tuple<Ts...>>
{
  using value_type = std::tuple<constant_value_or_ref_t<Ts>...>;

  template <typename... Us> static consteval value_type const& wrap(Us const&... items)
  {
    return make_object<value_type>(object_reflection(constant_wrapper<Us>::wrap(items))...);
  }

  static consteval value_type const& wrap(std::tuple<Ts...> const& v)
  {
    auto const& [... items] = v;
    return wrap(items...);
  }

  template <typename... Infos> static consteval value_type unwrap(Infos... r)
  {
    return value_type(extract<constant_value_or_ref_t<Ts>>(r)...);
  }
};

template <typename... Ts> struct constant_wrapper<std::variant<Ts...>>
{
  using value_type = std::variant<constant_value_or_ref_t<Ts>...>;
  static consteval value_type const& wrap(std::variant<Ts...> const& v)
  {
    template for(constexpr auto ii : std::views::iota(0uz, sizeof...(Ts)))
    {
      if(v.index() == ii)
      {
        using T = Ts...[ii];
        return make_object<value_type>(object_reflection(v.index()),
                                       object_reflection(constant_wrapper<std::decay_t<T>>::wrap(std::get<ii>(v))));
      }
    }
    std::unreachable();
  }
  template <meta::info ii, meta::info r> static consteval value_type unwrap()
  {
    return value_type(std::in_place_index<([:ii:])>, [:r:]);
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

  template <typename... Us>
  consteval constant(Us const&... v)
    requires requires { detail::constant_wrapper<T>::wrap(v...); }
      : value{detail::constant_wrapper<T>::wrap(v...)}
  {
  }

  constexpr value_type const& get() const
  {
    return value;
  }
  constexpr operator value_type const&() const
  {
    return value;
  }
  constexpr value_type const& operator*() const
  {
    return value;
  }
  constexpr value_type const* operator->() const
  {
    return std::addressof(value);
  }
};

// avoid extra overloading for string types, forwarded them to constant<std::string_view>
template <size_t N> constant(const char (&s)[N]) -> constant<std::string_view>;
constant(std::string const& s) -> constant<std::string_view>;

template <typename... Ts>
concept unique_type_pack_c = (std::same_as<Ts...[0], Ts> and ...);

template <typename... Ts>
concept non_unique_type_pack_c = not unique_type_pack_c<Ts...>;

template <typename... Ts>
  requires(sizeof...(Ts) > 1 and non_unique_type_pack_c<Ts...>)
constant(Ts const&...) -> constant<std::tuple<Ts...>>;

template <typename... Ts>
  requires(sizeof...(Ts) > 1 and unique_type_pack_c<Ts...>)
constant(Ts const&...) -> constant<std::array<Ts...[0], sizeof...(Ts)>>;

template <typename T>
  requires(is_structural_type(^^T))
struct constant<T>
{
  using value_type = T;
  value_type value;

  consteval constant(T const& v) : value{v}
  {
  }

  template <typename... Us>
  consteval constant(Us const&... v)
    requires requires { value_type{v...}; }
      : value{v...}
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
