#pragma once

#include <reflex/meta.hpp>

#include <optional>
#include <string_view>

namespace reflex::testing::detail
{
template <typename Fn> struct evaluator
{
  Fn               fn_;
  std::string_view expression_;

  static constexpr meta::info value_info = dealias(^^std::invoke_result_t<Fn>);
  using value_type                       = [:value_info:];
  static constexpr bool has_value        = value_info != ^^void;

  constexpr decltype(auto) value() const
  {
    if constexpr(has_value)
    {
      if constexpr(not is_reference_type(value_info))
      {
        static std::optional<value_type> v = std::nullopt;
        if(not v)
        {
          v = fn_();
        }
        return v.value();
      }
      else
      {
        return fn_();
      }
    }
    else
    {
      fn_();
    }
  }

  constexpr std::string_view str() const
  {
    return expression_;
  }
};
} // namespace reflex::testing::detail
