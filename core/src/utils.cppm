export module reflex.core:utils;

import :concepts;

import std;

export namespace reflex
{

constexpr bool is_in_range(int c, int low, int high)
{
  return (c >= low) and (c <= high);
}
constexpr bool is_cntrl(int c) noexcept
{
  return is_in_range(c, '\x00', '\x1f') //
      or c
      == '\x7f';
}
constexpr bool is_print(int c) noexcept
{
  return is_in_range(c, '\x20', '\x7e');
}
constexpr bool is_graph(int c) noexcept
{
  return is_in_range(c, '\x21', '\x7e');
}
constexpr bool is_blank(int c) noexcept
{
  return c == '\x09' or c == '\x20';
}
constexpr bool is_space(int c) noexcept
{
  return is_in_range(c, '\x09', '\x0d') //
      or c
      == '\x20';
}
constexpr bool is_upper(int c) noexcept
{
  return is_in_range(c, '\x41', '\x5a');
}
constexpr bool is_lower(int c) noexcept
{
  return is_in_range(c, '\x61', '\x7a');
}
constexpr bool is_alpha(int c) noexcept
{
  return is_upper(c) //
      or is_lower(c);
}
constexpr bool is_digit(int c) noexcept
{
  return is_in_range(c, '\x30', '\x39');
}
constexpr bool is_xdigit(int c) noexcept
{
  return is_digit(c)                    //
      or is_in_range(c, '\x41', '\x46') //
      or is_in_range(c, '\x61', '\x66');
}
constexpr bool is_alphanum(int c) noexcept
{
  return is_digit(c) or is_alpha(c);
}
constexpr bool is_punct(int c) noexcept
{
  return is_in_range(c, '\x21', '\x2f') //
      or is_in_range(c, '\x3a', '\x40') //
      or is_in_range(c, '\x5b', '\x60') //
      or is_in_range(c, '\x7b', '\x7e');
}

constexpr int to_lower(int c) noexcept
{
  if(is_upper(c))
  {
    return c + ('a' - 'A');
  }
  return c;
}
constexpr int to_upper(int c) noexcept
{
  if(is_lower(c))
  {
    return c - ('a' - 'A');
  }
  return c;
}

template <typename... Ts>
concept always_false = false;

namespace detail
{
template <typename Like, typename T> struct const_like_s;

template <typename Like, typename T>
  requires(not std::is_const_v<std::remove_reference_t<Like>>)
struct const_like_s<Like, T>
{
  using type = T;
};

template <typename Like, typename T>
  requires(std::is_const_v<std::remove_reference_t<Like>>)
struct const_like_s<Like, T>
{
  using type = std::add_const_t<T>;
};
} // namespace detail

template <typename Like, typename T>
using const_like_t = typename detail::const_like_s<Like, T>::type;

template <enum_c E> constexpr std::string_view enum_value_name(E value)
{
  template for(constexpr auto e : define_static_array(enumerators_of(^^E)))
  {
    if(value == [:e:])
    {
      return identifier_of(e);
    }
  }
  throw std::runtime_error("Invalid enum value");
}

template <enum_c E> constexpr std::optional<E> to_enum_value(std::string_view name)
{
  template for(constexpr auto e : std::define_static_array(std::meta::enumerators_of(^^E)))
  {
    if(name == std::meta::identifier_of(e))
    {
      return [:e:];
    }
  }

  return std::nullopt;
}

constexpr std::string_view trim(std::string_view s) noexcept
{
  while(!s.empty() and is_space(s.front()))
  {
    s.remove_prefix(1);
  }
  while(!s.empty() and is_space(s.back()))
  {
    s.remove_suffix(1);
  }
  return s;
}

} // namespace reflex

export template <reflex::enum_c E, typename CharT> struct std::formatter<E, CharT>
{
  constexpr auto parse(auto& ctx)
  {
    return ctx.begin();
  }

  constexpr auto format(const E& e, auto& ctx) const
  {
    return std::format_to(ctx.out(), "{}", reflex::enum_value_name(e));
  }
};
