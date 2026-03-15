export module reflex.core:constant_string;

import :meta;

import std;

export namespace reflex
{
namespace meta::detail
{
namespace representation_object
{

template <class Char, meta::info I>
constexpr std::basic_string_view<Char> string = [] {
  return std::basic_string_view<Char>(extract<Char const*>(I), extent(meta::type_of(I)) - 1);
}();

} // namespace representation_object

template <typename Char, typename... Infos> consteval decltype(auto) make_string(Infos... R)
{
  return extract<std::basic_string_view<Char> const&>(object_of(substitute(
      ^^representation_object::string, {
                                           ^^Char, R...})));
}

template <typename Char> consteval decltype(auto) wrap_string(std::basic_string_view<Char> const& s)
{
  return meta::detail::make_string<Char>(meta::reflect_constant(meta::reflect_constant_string(s)));
}

} // namespace meta::detail

template <typename Char> struct basic_constant_string
{
  using value_type = std::basic_string_view<Char>;

  value_type const& value;

  template <auto N>
  constexpr basic_constant_string(const Char (&str)[N])
      : value(meta::detail::wrap_string<Char>(str))
  {}

  constexpr basic_constant_string(std::basic_string_view<Char> str)
      : value(meta::detail::wrap_string<Char>(str))
  {}

  constexpr basic_constant_string(std::basic_string<Char> str)
      : value(meta::detail::wrap_string<Char>(str))
  {}

  constexpr value_type view() const noexcept
  {
    return value;
  }

  constexpr auto data() const noexcept
  {
    return value.data();
  }

  constexpr operator value_type() const noexcept
  {
    return value;
  }

  constexpr value_type operator*() const noexcept
  {
    return value;
  }

  constexpr bool operator==(basic_constant_string const& other) const noexcept
  {
    return value == other.value;
  }
  constexpr auto operator<=>(basic_constant_string const& other) const noexcept
  {
    return value <=> other.value;
  }
};

using constant_string = basic_constant_string<char>;

namespace literals
{

consteval constant_string operator""_sc(const char* data, std::size_t N)
{
  return {
      std::string_view{data, N}
  };
}

} // namespace literals

} // namespace reflex
