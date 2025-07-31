#pragma once

#include <reflex/meta.hpp>

namespace reflex::testing
{

template <typename T> struct expression
{
  T                value;
  std::string_view str;

  operator T&() noexcept
  {
    return value;
  }
  operator T const&() const& noexcept
  {
    return value;
  }
};

// template <> struct expression<std::meta::info>
// {
//   std::meta::info  value;
//   std::string_view str;

//   consteval operator std::meta::info() noexcept
//   {
//     return value;
//   }
// };

consteval bool is_expression(meta::info R)
{
  return has_template_arguments(decay(R)) and template_of(decay(R)) == ^^expression;
}

#define expr(_expr_) reflex::testing::expression((_expr_), #_expr_)
} // namespace reflex::testing

template <typename T, typename CharT>
struct std::formatter<reflex::testing::expression<T>, CharT> : std::formatter<T, CharT>
{
  using super = std::formatter<T, CharT>;

  decltype(auto) format(reflex::testing::expression<T> const& r, auto& ctx) const
  {
    return super::format(r.value, ctx);
  }
};
