#pragma once

#include <format>
#include <memory>

template <typename T, typename CharT>
struct std::formatter<std::shared_ptr<T>, CharT> : std::formatter<std::decay_t<T>, CharT>
{
  auto format(std::shared_ptr<T> const& r, auto& ctx) const
  {
    return std::formatter<std::decay_t<T>, CharT>::format(*r, ctx);
  }
};

template <typename T, typename CharT>
struct std::formatter<std::unique_ptr<T>, CharT> : std::formatter<std::decay_t<T>, CharT>
{
  auto format(std::unique_ptr<T> const& r, auto& ctx) const
  {
    return std::formatter<std::decay_t<T>, CharT>::format(*r, ctx);
  }
};
