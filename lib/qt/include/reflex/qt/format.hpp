#pragma once

#include <QString>

#include <format>

template <typename CharT> struct std::formatter<QString, CharT> : std::formatter<std::basic_string_view<CharT>, CharT>
{
  auto format(QString const& s, auto& ctx) const
  {
    if constexpr(^^CharT == ^^wchar_t)
    {
      return std::format_to(ctx.out(), "{}", s.data());
    }
    else
    {
      const auto data = s.toUtf8();
      return std::format_to(ctx.out(), "{}", data.data());
    }
  }
};

