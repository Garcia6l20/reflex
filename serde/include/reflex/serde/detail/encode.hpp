#pragma once

#ifndef REFLEX_MODULE
#include <cstddef>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
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

constexpr int base64_digit(char c)
{
  if(c >= 'A' and c <= 'Z')
  {
    return c - 'A';
  }
  if(c >= 'a' and c <= 'z')
  {
    return c - 'a' + 26;
  }
  if(c >= '0' and c <= '9')
  {
    return c - '0' + 52;
  }
  if(c == '+')
  {
    return 62;
  }
  if(c == '/')
  {
    return 63;
  }
  return -1;
}

constexpr std::optional<std::vector<std::byte>> base64_decode(std::string_view text)
{
  if(text.size() % 4 != 0)
  {
    return std::nullopt;
  }
  std::vector<std::byte> out;
  out.reserve(text.size() / 4 * 3);
  for(std::size_t i = 0; i < text.size(); i += 4)
  {
    const bool last  = i + 4 == text.size();
    const auto pad   = last ? (text[i + 3] == '=' ? (text[i + 2] == '=' ? 2u : 1u) : 0u) : 0u;
    unsigned   group = 0;
    for(std::size_t j = 0; j < 4; ++j)
    {
      int digit = 0;
      if(j < 4 - pad)
      {
        digit = base64_digit(text[i + j]);
        if(digit < 0)
        {
          return std::nullopt;
        }
      }
      group = group << 6 | static_cast<unsigned>(digit);
    }
    if((pad == 1 and (group & 0xFF) != 0) or (pad == 2 and (group & 0xFFFF) != 0))
    {
      return std::nullopt;
    }
    out.push_back(static_cast<std::byte>(group >> 16 & 0xFF));
    if(pad < 2)
    {
      out.push_back(static_cast<std::byte>(group >> 8 & 0xFF));
    }
    if(pad < 1)
    {
      out.push_back(static_cast<std::byte>(group & 0xFF));
    }
  }
  return out;
}

constexpr int hex_digit(char c)
{
  if(c >= '0' and c <= '9')
  {
    return c - '0';
  }
  if(c >= 'a' and c <= 'f')
  {
    return c - 'a' + 10;
  }
  if(c >= 'A' and c <= 'F')
  {
    return c - 'A' + 10;
  }
  return -1;
}

constexpr bool hex_decode(std::string_view text, std::span<std::byte> out)
{
  if(text.size() != out.size() * 2)
  {
    return false;
  }
  for(std::size_t i = 0; i < out.size(); ++i)
  {
    const int high = hex_digit(text[2 * i]);
    const int low  = hex_digit(text[2 * i + 1]);
    if(high < 0 or low < 0)
    {
      return false;
    }
    out[i] = static_cast<std::byte>(high << 4 | low);
  }
  return true;
}
}
