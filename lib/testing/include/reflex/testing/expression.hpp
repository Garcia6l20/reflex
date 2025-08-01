#pragma once

#include <reflex/meta.hpp>

namespace reflex::testing
{

template <typename T> struct expression
{
  T                value;
  std::string_view str;

  decltype(auto) get(this auto& self)
  {
    return self.value;
  }

  operator T&() noexcept
  {
    return value;
  }
  operator T const&() const& noexcept
  {
    return value;
  }
};

template <typename T> struct expression<std::reference_wrapper<T>>
{
  std::reference_wrapper<T> value;
  std::string_view          str;

  decltype(auto) get(this auto& self)
  {
    return self.value.get();
  }

  operator T&() noexcept
  {
    return value.get();
  }
  operator T const&() const& noexcept
  {
    return value.get();
  }
};

consteval bool is_expression(meta::info R)
{
  return has_template_arguments(decay(R)) and template_of(decay(R)) == ^^expression;
}

#define expr(...) reflex::testing::expression((__VA_ARGS__), #__VA_ARGS__)
} // namespace reflex::testing


template <typename T, typename CharT>
struct std::formatter<std::reference_wrapper<T>, CharT> : std::formatter<std::decay_t<T>, CharT>
{
  auto format(std::reference_wrapper<T> const& r, auto& ctx) const
  {
    return std::formatter<std::decay_t<T>, CharT>::format(r.get(), ctx);
  }
};

template <typename T, typename CharT>
struct std::formatter<reflex::testing::expression<T>, CharT> : std::formatter<T, CharT>
{
  using super = std::formatter<T, CharT>;

  decltype(auto) format(reflex::testing::expression<T> const& r, auto& ctx) const
  {
    return super::format(r.get(), ctx);
  }
};
