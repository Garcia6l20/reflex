#pragma once

#include <reflex/meta.hpp>

namespace reflex
{
template <typename Char> struct basic_constant_string
{
  using value_type = std::basic_string_view<Char>;

  const Char* data_ = nullptr;
  std::size_t size_ = 0;

  constexpr basic_constant_string() = default;

  template <auto N> constexpr basic_constant_string(const Char (&str)[N])
  {
    const auto ss = std::basic_string_view<Char>(
        define_static_string(std::basic_string_view<Char>(str, N - 1)));
    data_ = ss.data();
    size_ = ss.size();
  }

  constexpr basic_constant_string(std::basic_string_view<Char> str)
  {
    const auto ss = std::basic_string_view<Char>(define_static_string(str));
    data_         = ss.data();
    size_         = ss.size();
  }

  constexpr basic_constant_string(std::basic_string<Char> str)
  {
    const auto ss =
        std::basic_string_view<Char>(define_static_string(std::basic_string_view<Char>(str)));
    data_ = ss.data();
    size_ = ss.size();
  }

  constexpr value_type view() const noexcept
  {
    return value_type(data_, size_);
  }

  constexpr auto data() const noexcept
  {
    return data_;
  }

  constexpr auto size() const noexcept
  {
    return size_;
  }

  constexpr auto empty() const noexcept
  {
    return size_ == 0;
  }

  constexpr operator value_type() const noexcept
  {
    return view();
  }

  constexpr value_type operator*() const noexcept
  {
    return view();
  }

  constexpr bool operator==(basic_constant_string const& other) const noexcept
  {
    return view() == other.view();
  }
  constexpr auto operator<=>(basic_constant_string const& other) const noexcept
  {
    return view() <=> other.view();
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

namespace std
{
template <typename Char>
struct formatter<reflex::basic_constant_string<Char>>
    : std::formatter<std::basic_string_view<Char>, Char>
{
  constexpr auto format(reflex::basic_constant_string<Char> const& str, auto& ctx) const
  {
    return std::formatter<std::basic_string_view<Char>, Char>::format(str.view(), ctx);
  }
};
} // namespace std
