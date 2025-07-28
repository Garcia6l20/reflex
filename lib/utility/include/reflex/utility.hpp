#pragma once

namespace reflex
{

constexpr bool is_in_range(int c, int low, int high)
{
  return (c >= low) and (c <= high);
}
constexpr bool is_cntrl(int c) noexcept
{
  return is_in_range(c, '\x00', '\x1f') //
         or c == '\x7f';
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
         or c == '\x20';
}
constexpr bool is_upper(int c) noexcept
{
  return is_in_range(c, '\x41', '\x5a');
}
constexpr bool is_alpha(int c) noexcept
{
  return is_upper(c) //
         or is_in_range(c, '\x61', '\x7a');
}
constexpr bool is_digit(int c) noexcept
{
  return is_in_range(c, '\x30', '\x39');
}
constexpr bool is_xdigit(int c) noexcept
{
  return is_digit(c)                       //
         or is_in_range(c, '\x41', '\x46') //
         or is_in_range(c, '\x61', '\x66');
}
constexpr bool is_alphanum(int c) noexcept
{
  return is_digit(c) or is_alpha(c);
}
constexpr bool is_punct(int c) noexcept
{
  return is_in_range(c, '\x21', '\x2f')    //
         or is_in_range(c, '\x3a', '\x40') //
         or is_in_range(c, '\x5b', '\x60') //
         or is_in_range(c, '\x7b', '\x7e');
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

template <typename Like, typename T> using const_like_t = typename detail::const_like_s<Like, T>::type;

} // namespace reflex