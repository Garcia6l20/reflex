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

template <typename T>
  requires(is_structural_type(^^T))
struct constant_wrapper<T>
{
  using value_type = T;
};

namespace representation_object
{
template <class T, std::meta::info... Is> inline constexpr constant_value_t<T> array[] = {[:Is:]...};
template <class T, std::meta::info... Is>
inline constexpr constant_value_t<T> object = [] { return constant_wrapper<T>::unwrap(Is...); }();
} // namespace representation_object

template <typename Str> struct string_wrapper
{
  using T          = Str;
  using value_type = std::string_view;
  static consteval decltype(auto) wrap(T const& s)
  {
    return extract<value_type const&>(
        object_of(substitute(^^representation_object::object,
                             {
                                 ^^T,
                                 std::meta::reflect_constant(std::meta::reflect_constant_string(s))})));
  }
  static consteval value_type unwrap(std::meta::info r)
  {
    return value_type(extract<char const*>(r), extent(type_of(r)) - 1);
  }
};

template <> struct constant_wrapper<std::string> : string_wrapper<std::string>
{
};
template <> struct constant_wrapper<std::string_view> : string_wrapper<std::string_view>
{
};

template <size_t N> struct constant_wrapper<const char[N]> : string_wrapper<const char[N]>
{
};
template <size_t N> struct constant_wrapper<char[N]> : string_wrapper<char[N]>
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
    return extract<value_type const&>(
        object_of(substitute(^^representation_object::object,
                             {
                                 ^^R,
                                 std::meta::reflect_constant(reflect_constant_array(v))})));
  }

  static consteval value_type unwrap(std::meta::info r)
  {
    return value_type(extract<element_type const*>(r), extent(type_of(r)));
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

template <size_t N> using string_literal_constant = constant<const char[N]>;
} // namespace meta

template <typename T> using constant = meta::constant<T>;

template <size_t N> using string_literal_constant = meta::string_literal_constant<N>;

} // namespace reflex
