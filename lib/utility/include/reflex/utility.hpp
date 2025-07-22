#pragma once

namespace reflex
{
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