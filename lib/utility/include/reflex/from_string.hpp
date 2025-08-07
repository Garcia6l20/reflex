#pragma once

#include <reflex/meta.hpp>

#include <charconv>
#include <expected>
#include <system_error>

namespace reflex
{
template <typename T> struct string_loader;

template <typename T> using from_string_result = std::expected<T, std::system_error>;

template <typename T>
concept arithmetic_c = std::is_arithmetic_v<T>;

template <arithmetic_c T> struct string_loader<T>
{
  constexpr from_string_result<T> from_string(std::string_view sv) const
  {
    T value;
    const auto [ptr, ec] = std::from_chars(sv.begin(), sv.begin() + sv.size(), value);
    if(ec != std::errc{})
    {
      return std::unexpected(std::system_error(int(ec), std::system_category()));
    }
    return value;
  }
};

template <typename Optional>
  requires(meta::is_template_instance_of(^^Optional, ^^std::optional))
struct string_loader<Optional>
{
  using value_type = typename Optional::value_type;

  constexpr from_string_result<value_type> from_string(std::string_view sv) const
  {
    return string_loader<value_type>{}.from_string(sv);
  }
};

template <> struct string_loader<std::string_view>
{
  constexpr from_string_result<std::string_view> from_string(std::string_view sv) const
  {
    return sv;
  }
};

template <> struct string_loader<std::string>
{
  constexpr from_string_result<std::string> from_string(std::string_view s) const
  {
    return std::string{s};
  }
};

template <typename T> auto from_string(std::string_view s) noexcept
{
  return string_loader<T>{}.from_string(s);
}

} // namespace reflex
