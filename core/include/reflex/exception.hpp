#pragma once

#include <stdexcept>
#include <format>

namespace reflex
{

struct runtime_error : std::runtime_error
{
  using std::runtime_error::runtime_error;

  template <typename... Args>
  runtime_error(std::format_string<Args...>&& fmt, Args&&... args)
      : std::runtime_error(std::format(std::forward<decltype(fmt)>(fmt), std::forward<Args>(args)...))
  {}
};

struct invalid_argument : std::invalid_argument
{
  using std::invalid_argument::invalid_argument;

  template <typename... Args>
  invalid_argument(std::format_string<Args...>&& fmt, Args&&... args)
      : std::invalid_argument(std::format(std::forward<decltype(fmt)>(fmt), std::forward<Args>(args)...))
  {}
};

struct logic_error : std::logic_error
{
  using std::logic_error::logic_error;

  template <typename... Args>
  logic_error(std::format_string<Args...>&& fmt, Args&&... args)
      : std::logic_error(std::format(std::forward<decltype(fmt)>(fmt), std::forward<Args>(args)...))
  {}
};

} // namespace reflex
