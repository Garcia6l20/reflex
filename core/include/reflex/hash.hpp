#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <concepts>
#include <cstddef>
#include <string_view>
#include <utility>
#endif

#include <reflex/constant.hpp>
#include <reflex/tag_invoke.hpp>

REFLEX_EXPORT namespace reflex
{
constexpr std::size_t hash_bytes(const char* ptr, std::size_t len,
                                 std::size_t seed = static_cast<std::size_t>(0xc70f6907UL))
{
#if __SIZEOF_SIZE_T__ == 8
  // Loads n bytes (1 <= n <= 8) little-end first, like load_bytes/unaligned_load.
  auto load_n = [](const char* p, std::size_t n) -> std::size_t {
    std::size_t result = 0;
    for(std::size_t i = n; i-- > 0;)
    {
      result = (result << 8) + static_cast<unsigned char>(p[i]);
    }
    return result;
  };
  auto shift_mix = [](std::size_t v) -> std::size_t { return v ^ (v >> 47); };

  const std::size_t mul = (static_cast<std::size_t>(0xc6a4a793UL) << 32UL) + static_cast<std::size_t>(0x5bd1e995UL);
  const std::size_t len_aligned = len & ~static_cast<std::size_t>(0x7);
  const char* const end = ptr + len_aligned;
  std::size_t hash = seed ^ (len * mul);
  for(const char* p = ptr; p != end; p += 8)
  {
    const std::size_t data = shift_mix(load_n(p, 8) * mul) * mul;
    hash ^= data;
    hash *= mul;
  }
  if((len & 0x7) != 0)
  {
    const std::size_t data = load_n(end, len & 0x7);
    hash ^= data;
    hash *= mul;
  }
  hash = shift_mix(hash) * mul;
  hash = shift_mix(hash);
  return hash;
#elif __SIZEOF_SIZE_T__ == 4
  auto load4 = [](const char* p) -> std::size_t {
    std::size_t result = 0;
    for(std::size_t i = 4; i-- > 0;)
    {
      result = (result << 8) + static_cast<unsigned char>(p[i]);
    }
    return result;
  };

  const std::size_t m = 0x5bd1e995;
  std::size_t hash = seed ^ len;
  const char* buf = ptr;
  while(len >= 4)
  {
    std::size_t k = load4(buf);
    k *= m;
    k ^= k >> 24;
    k *= m;
    hash *= m;
    hash ^= k;
    buf += 4;
    len -= 4;
  }
  switch(len)
  {
  case 3:
    hash ^= static_cast<std::size_t>(static_cast<unsigned char>(buf[2])) << 16;
    [[fallthrough]];
  case 2:
    hash ^= static_cast<std::size_t>(static_cast<unsigned char>(buf[1])) << 8;
    [[fallthrough]];
  case 1:
    hash ^= static_cast<std::size_t>(static_cast<unsigned char>(buf[0]));
    hash *= m;
  }
  hash ^= hash >> 13;
  hash *= m;
  hash ^= hash >> 15;
  return hash;
#else
  // Mirrors the library's fallback for unusual sizeof(size_t).
  std::size_t hash = seed;
  for(std::size_t i = 0; i < len; ++i)
  {
    hash = (hash * 131) + static_cast<unsigned char>(ptr[i]);
  }
  return hash;
#endif
}

// hash CPO: mirrors std::hash for the supported subset, extensible via
// tag_invoke. Built-in overloads cover integral/bool/char-likes/enum (the
// library's trivial value-identity specializations) and string-likes (hashed
// with hash_bytes, seed 0xc70f6907). Add support for a user type T by defining
// `std::size_t tag_invoke(reflex::tag_t<reflex::hash>, T const&)` in T's
// namespace.
inline constexpr struct __hash_tag : customization_point_object
{
} hash;

template <std::integral T>
constexpr std::size_t tag_invoke(tag_t<hash>, T v) noexcept
{
  return static_cast<std::size_t>(v);
}

template <enum_c T>
constexpr std::size_t tag_invoke(tag_t<hash>, T v) noexcept
{
    return hash(std::to_underlying(v));
}

constexpr std::size_t tag_invoke(tag_t<hash>, std::string_view s) noexcept
{
  return hash_bytes(s.data(), s.size());
}

consteval std::size_t tag_invoke(tag_t<hash>, std::meta::info const& info) noexcept
{
  auto id = std::meta::display_string_of(info);
  return hash_bytes(id.data(), id.size());
}

// constant_string and other string NTTP wrappers: deref to the held value.
template <typename T>
  requires(not std::convertible_to<T, std::string_view>) and requires(T const& c) { std::string_view{*c}; }
constexpr std::size_t tag_invoke(tag_t<hash>, T const& c) noexcept
{
  return hash_bytes(std::string_view{*c}.data(), std::string_view{*c}.size());
}

template <typename T>
concept hashable_c = tag_invocable_c<tag_t<hash>, T>;
}
