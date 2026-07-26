#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <charconv>
#include <format>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#endif

#include <reflex/diags.hpp>

REFLEX_EXPORT namespace reflex
{
  struct assertion_error : std::logic_error
  {
    using std::logic_error::logic_error;
  };

  consteval void const_assert(
      bool b, std::string_view description = "",
      std::source_location loc = std::source_location::current())
  {
    if(!b)
    {
      // std::format is not usable in a constant expression under libstdc++, so
      // the message an assertion reports has to be assembled by hand. Doing it
      // with std::format is what an assertion that fires used to die on.
      char line[24];
      auto [line_end, _] = std::to_chars(line, line + sizeof(line), loc.line());

      std::string message = "Assertion failed at: ";
      message += loc.file_name();
      message += ':';
      message.append(line, line_end);
      message += " (";
      message += loc.function_name();
      message += ')';
      if(!description.empty())
      {
        message += ": ";
        message += description;
      }
      __builtin_constexpr_diag(
          int(diags::severity::error | diags::severity::parent_location), "", message);
    }
  }

  template <typename... Args>
  consteval void const_assert(bool b, std::format_string<Args...> fmt, Args&&... args)
  {
    if(!b)
    {
      __builtin_constexpr_diag(
          int(diags::severity::error | diags::severity::parent_location), "",
          std::format(fmt, std::forward<Args>(args)...));
    }
  }
} // namespace reflex
