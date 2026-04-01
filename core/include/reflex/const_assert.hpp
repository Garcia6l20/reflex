#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <format>
#include <source_location>
#include <stdexcept>
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
      using namespace std::string_literals;
      auto message = std::format(
          "Assertion failed at: {}:{} ({})", loc.file_name(), loc.line(), loc.function_name());
      if(!description.empty())
      {
        std::format_to(std::back_inserter(message), ": {}", description);
      }
      __builtin_constexpr_diag(
          int(diags::severity::error | diags::severity::parent_location), "", message);
    }
  }
} // namespace reflex