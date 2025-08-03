#pragma once

#include <format>
#include <functional>

template <typename T, typename CharT>
struct std::formatter<std::reference_wrapper<T>, CharT> : std::formatter<std::decay_t<T>, CharT>
{
  auto format(std::reference_wrapper<T> const& r, auto& ctx) const
  {
    return std::formatter<std::decay_t<T>, CharT>::format(r.get(), ctx);
  }
};
