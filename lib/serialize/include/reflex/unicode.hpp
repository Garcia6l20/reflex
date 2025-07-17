#pragma once

#include <charconv>
#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include <string_view>

namespace reflex::unicode
{
constexpr std::tuple<uint32_t, size_t> from_utf8(std::string_view utf8)
{
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(utf8.data());
  const size_t   len   = utf8.size();

  if(len == 0)
  {
    return {0, 0};
  }

  int codepoint = 0;

  if((bytes[0] & 0x80) == 0)
  {
    // 1 byte: 0xxxxxxx
    return {bytes[0], 1};
  }
  else if((bytes[0] & 0xE0) == 0xC0 && len >= 2)
  {
    // 2 bytes: 110xxxxx 10xxxxxx
    return {((bytes[0] & 0x1F) << 6) | (bytes[1] & 0x3F), 2};
  }
  else if((bytes[0] & 0xF0) == 0xE0 && len >= 3)
  {
    // 3 bytes: 1110xxxx 10xxxxxx 10xxxxxx
    return {((bytes[0] & 0x0F) << 12) | ((bytes[1] & 0x3F) << 6) | (bytes[2] & 0x3F), 3};
  }
  else if((bytes[0] & 0xF8) == 0xF0 && len >= 4)
  {
    // 4 bytes: 11110xxx 10xxxxxx 10xxxxxx 10xxxxxx
    return {((bytes[0] & 0x07) << 18) | ((bytes[1] & 0x3F) << 12) | ((bytes[2] & 0x3F) << 6) | (bytes[3] & 0x3F), 4};
  }

  return {0, 0};
}

constexpr size_t to_utf8(auto& out, int codepoint) noexcept
{
  if(codepoint <= 0x7F)
  {
    out = static_cast<char>(codepoint);
    return 1;
  }
  else if(codepoint <= 0x7FF)
  {
    out = static_cast<char>(0xC0 | (codepoint >> 6));
    out = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 2;
  }
  else if(codepoint <= 0xFFFF)
  {
    out = static_cast<char>(0xE0 | (codepoint >> 12));
    out = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 3;
  }
  else if(codepoint <= 0x10FFFF)
  {
    out = static_cast<char>(0xF0 | (codepoint >> 18));
    out = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
    out = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
    out = static_cast<char>(0x80 | (codepoint & 0x3F));
    return 4;
  }
  else
  {
    return 0;
  }
}

constexpr bool escape(auto out, std::string_view utf8) noexcept
{
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(utf8.data());
  const size_t   len   = utf8.size();
  for(size_t ii = 0; ii < len;)
  {
    const size_t remaining            = len - ii;
    const auto [encoded, encoded_len] = from_utf8(std::string_view{utf8.data() + ii, remaining});
    if(encoded_len == 0)
    {
      return false; // invalid character
    }
    else if(encoded_len > 1)
    {
      std::format_to(out, "\\u{:04x}", encoded);
    }
    else
    {
      out = char(encoded);
    }
    ii += encoded_len;
  }
  return true;
}

constexpr std::string escape(std::string_view utf8) noexcept
{
  std::string result;
  result.reserve(utf8.size());
  escape(std::back_inserter(result), utf8);
  return result;
}

constexpr bool unescape(auto out, std::string_view utf8) noexcept
{
  const uint8_t* bytes = reinterpret_cast<const uint8_t*>(utf8.data());
  const size_t   len   = utf8.size();
  for(size_t ii = 0; ii < len;)
  {
    const size_t remaining = len - ii;
    if(remaining > 2 and bytes[ii] == '\\' and bytes[ii + 1] == 'u')
    {
      uint16_t code;
      auto [ptr, ec] = std::from_chars(&utf8.data()[ii + 2], &utf8.data()[ii + 7], code, 16);
      if(ec != std::errc{})
      {
        return false;
      }
      if(auto count = to_utf8(out, code); count == 0)
      {
        return false;
      }
      else
      {
        ii += 6;
      }
    }
    else
    {
      out = utf8.data()[ii++];
    }
  }
  return true;
}

constexpr std::string unescape(std::string_view utf8) noexcept
{
  std::string result;
  result.reserve(utf8.size() / 2); // random
  unescape(std::back_inserter(result), utf8);
  return result;
}

} // namespace reflex::unicode
