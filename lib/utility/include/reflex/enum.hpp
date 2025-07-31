#pragma once

#include <type_traits>

namespace reflex
{

template <typename T>
concept enum_c = std::is_enum_v<std::decay_t<T>>;

namespace enum_bitwise_operators
{

template <enum_c E> constexpr E operator~(E rhs) noexcept
{
  return static_cast<E>(~static_cast<std::underlying_type_t<E>>(rhs));
}

template <enum_c E> constexpr E operator|(E lhs, E rhs) noexcept
{
  return static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) | static_cast<std::underlying_type_t<E>>(rhs));
}

template <enum_c E> constexpr E operator&(E lhs, E rhs) noexcept
{
  return static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) & static_cast<std::underlying_type_t<E>>(rhs));
}

template <enum_c E> constexpr E operator^(E lhs, E rhs) noexcept
{
  return static_cast<E>(static_cast<std::underlying_type_t<E>>(lhs) ^ static_cast<std::underlying_type_t<E>>(rhs));
}

template <enum_c E> constexpr E& operator|=(E& lhs, E rhs) noexcept
{
  return lhs = (lhs | rhs);
}

template <enum_c E> constexpr E& operator&=(E& lhs, E rhs) noexcept
{
  return lhs = (lhs & rhs);
}

template <enum_c E> constexpr E& operator^=(E& lhs, E rhs) noexcept
{
  return lhs = (lhs ^ rhs);
}
} // namespace enum_bitwise_operators
} // namespace reflex