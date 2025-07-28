#pragma once

#include <type_traits>
#include <format>

namespace reflex
{
struct none_t
{
  constexpr none_t() = default;

  constexpr none_t(none_t&&)            = default;
  constexpr none_t& operator=(none_t&&) = default;

  constexpr none_t(none_t const&)            = default;
  constexpr none_t& operator=(none_t const&) = default;

  constexpr operator bool() const noexcept
  {
    return false;
  }
  constexpr bool operator==(none_t const&) const noexcept
  {
    return true;
  }
  constexpr bool operator==(auto const&) const noexcept
  {
    return false;
  }
};

inline constexpr none_t none;

template <typename T> struct is_none_fn : std::false_type
{
};

template <> struct is_none_fn<none_t> : std::true_type
{
};

template <typename T>
concept none_c = is_none_fn<std::decay_t<T>>::value;

template <typename T> constexpr bool is_none(T&&)
{
  return none_c<std::decay_t<T>>;
}

template <typename T> struct void_is_none
{
  using type = T;
};

template <> struct void_is_none<void>
{
  using type = none_t;
};

template <typename T> using void_is_none_t = typename void_is_none<T>::type;

} // namespace reflex

template <typename CharT> struct std::formatter<reflex::none_t, CharT> : formatter<std::basic_string_view<CharT>, CharT>
{
  using super = formatter<std::basic_string_view<CharT>, CharT>;

  template <class FmtContext> FmtContext::iterator format(reflex::none_t const&, FmtContext& ctx) const
  {
    return super::format("none", ctx);
  }
};
