#pragma once

#ifndef REFLEX_MODULE
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#endif

namespace reflex::serde::detail
{
constexpr std::string base64_encode(std::span<std::byte const> bytes)
{
  constexpr std::string_view alphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

  std::string out;
  out.reserve((bytes.size() + 2) / 3 * 4);
  std::size_t i = 0;
  for(; i + 3 <= bytes.size(); i += 3)
  {
    const auto group = (std::to_integer<unsigned>(bytes[i]) << 16)
                     | (std::to_integer<unsigned>(bytes[i + 1]) << 8)
                     | std::to_integer<unsigned>(bytes[i + 2]);
    out.push_back(alphabet[group >> 18 & 0x3F]);
    out.push_back(alphabet[group >> 12 & 0x3F]);
    out.push_back(alphabet[group >> 6 & 0x3F]);
    out.push_back(alphabet[group & 0x3F]);
  }
  if(const auto rest = bytes.size() - i; rest != 0)
  {
    auto group = std::to_integer<unsigned>(bytes[i]) << 16;
    if(rest == 2)
    {
      group |= std::to_integer<unsigned>(bytes[i + 1]) << 8;
    }
    out.push_back(alphabet[group >> 18 & 0x3F]);
    out.push_back(alphabet[group >> 12 & 0x3F]);
    out.push_back(rest == 2 ? alphabet[group >> 6 & 0x3F] : '=');
    out.push_back('=');
  }
  return out;
}

constexpr std::string hex_encode(std::span<std::byte const> bytes)
{
  constexpr std::string_view digits = "0123456789abcdef";

  std::string out;
  out.reserve(bytes.size() * 2);
  for(auto b : bytes)
  {
    const auto v = std::to_integer<unsigned>(b);
    out.push_back(digits[v >> 4]);
    out.push_back(digits[v & 0x0F]);
  }
  return out;
}
}
