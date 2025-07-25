#pragma once

#include <algorithm>
#include <string_view>

namespace reflex
{
template <size_t N> struct fixed_string
{
  char data[N + 1];

  constexpr fixed_string(char const* s)
  {
    std::copy(s, s + N, data);
    data[N] = '\0';
  }

  constexpr auto view() const -> std::string_view
  {
    return {data};
  }

  constexpr auto operator[](size_t i) const
  {
    return data[i];
  }

  constexpr auto find(auto needle) const
  {
    return view().find(needle);
  }

  constexpr auto size() const
  {
    return N;
  }
};
template <size_t N> fixed_string(const char (&s)[N]) -> fixed_string<N>;

template <fixed_string input> constexpr auto operator""_fs()
{
  return input;
}

template <fixed_string s> static constexpr auto extract()
{
  return s.view();
}

template <fixed_string s, long long begin, long long end> static constexpr auto extract()
{
  if constexpr(end < 0)
  {
    return s.view().substr(begin, (s.size() - 1 + end) - begin);
  }
  else
  {
    return s.view().substr(begin, end);
  }
}

} // namespace reflex