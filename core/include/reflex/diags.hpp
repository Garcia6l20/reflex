#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <format>
#endif

#include <reflex/enum.hpp>

REFLEX_EXPORT namespace reflex::diags
{
  enum class[[= reflex::enum_flags]] severity
  {
    info            = 0b0'0000,
    warning         = 0b0'0001,
    error           = 0b0'0010,
    parent_location = 0b1'0000
  };

  template <typename... Args>
  constexpr void error(std::format_string<Args...> fmt, Args && ... args)
  {
    __builtin_constexpr_diag(
        int(severity::error | severity::parent_location), "",
        std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args>
  constexpr void warning(std::format_string<Args...> fmt, Args && ... args)
  {
    __builtin_constexpr_diag(
        int(severity::warning | severity::parent_location), "",
        std::format(fmt, std::forward<Args>(args)...));
  }
  template <typename... Args> constexpr void info(std::format_string<Args...> fmt, Args && ... args)
  {
    __builtin_constexpr_diag(
        int(severity::info | severity::parent_location), "",
        std::format(fmt, std::forward<Args>(args)...));
  }
}
