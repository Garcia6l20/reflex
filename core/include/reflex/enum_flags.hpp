#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/concepts.hpp>
#include <reflex/meta.hpp>

REFLEX_EXPORT namespace reflex
{
  /// @brief An annotation that can be applied to enum types to indicate that they should be treated
  /// as bit flags.
  constexpr struct __enum_flags
  {
  } enum_flags;

  template <typename E>
  concept enum_flags_c = enum_c<E> and meta::has_annotation(^^E, ^^enum_flags);

  template <enum_flags_c EF> constexpr auto operator|(EF lhs, EF rhs) noexcept -> EF
  {
    using underlying = std::underlying_type_t<EF>;
    return static_cast<EF>(static_cast<underlying>(lhs) | static_cast<underlying>(rhs));
  }

  template <enum_flags_c EF> constexpr auto operator&(EF lhs, EF rhs) noexcept -> EF
  {
    using underlying = std::underlying_type_t<EF>;
    return static_cast<EF>(static_cast<underlying>(lhs) & static_cast<underlying>(rhs));
  }

  template <enum_flags_c EF> constexpr auto operator^(EF lhs, EF rhs) noexcept -> EF
  {
    using underlying = std::underlying_type_t<EF>;
    return static_cast<EF>(static_cast<underlying>(lhs) ^ static_cast<underlying>(rhs));
  }

  template <enum_flags_c EF> constexpr auto operator~(EF value) noexcept -> EF
  {
    using underlying = std::underlying_type_t<EF>;
    return static_cast<EF>(~static_cast<underlying>(value));
  }

  template <enum_flags_c EF> constexpr auto operator|=(EF& lhs, EF rhs) noexcept -> EF&
  {
    lhs = lhs | rhs;
    return lhs;
  }

  template <enum_flags_c EF> constexpr auto operator&=(EF& lhs, EF rhs) noexcept -> EF&
  {
    lhs = lhs & rhs;
    return lhs;
  }

  template <enum_flags_c EF> constexpr auto operator^=(EF& lhs, EF rhs) noexcept -> EF&
  {
    lhs = lhs ^ rhs;
    return lhs;
  }
} // namespace reflex
