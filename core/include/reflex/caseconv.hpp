#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/utils.hpp>

REFLEX_EXPORT namespace reflex::caseconv
{
template <typename OutputT = std::string> constexpr OutputT to_snake_case(std::string_view str)
{
  OutputT result;
  if constexpr(requires { result.reserve(str.size()); })
  {
    result.reserve(str.size());
  }
  for(std::size_t i = 0; i < str.size(); ++i)
  {
    char c = str[i];
    if(is_upper(c))
    {
      if(i > 0 and is_lower(str[i - 1]))
      {
        result += '_';
      }
      result += char(to_lower(c));
    }
    else if(c == '-')
    {
      result += '_';
    }
    else
    {
      result += c;
    }
  }
  return result;
}
template <typename OutputT = std::string> constexpr OutputT to_pascal_case(std::string_view str)
{
  OutputT result;
  if constexpr(requires { result.reserve(str.size()); })
  {
    result.reserve(str.size());
  }
  bool capitalize_next = true;
  for(char c : str)
  {
    if(is_alphanum(c))
    {
      if(capitalize_next)
      {
        result += char(to_upper(c));
        capitalize_next = false;
      }
      else
      {
        result += c;
      }
    }
    else
    {
      capitalize_next = true;
    }
  }
  return result;
}

template <typename OutputT = std::string> constexpr OutputT to_kebab_case(std::string_view str)
{
  OutputT result = to_snake_case<OutputT>(str);
  for(char& c : result)
  {
    if(c == '_')
    {
      c = '-';
    }
  }
  return result;
}

template <typename OutputT = std::string> constexpr OutputT to_camel_case(std::string_view str)
{
  OutputT result = to_pascal_case<OutputT>(str);
  if(result.empty())
  {
    return result;
  }
  using value_type = typename OutputT::value_type;
  result[0]        = value_type(to_lower(result[0]));
  return result;
}

template <typename OutputT = std::string> constexpr OutputT to_upper(std::string_view str)
{
  return str | std::views::transform(reflex::to_upper) | std::ranges::to<OutputT>();
}

template <typename OutputT = std::string> constexpr OutputT to_lower(std::string_view str)
{
  return str | std::views::transform(reflex::to_lower) | std::ranges::to<OutputT>();
}

enum class naming
{
  lower,
  upper,
  camel_case,
  snake_case,
  kebab_case,
  pascal_case
};

template <typename OutputT = std::string> constexpr OutputT to_case(std::string_view str, naming n)
{
  switch(n)
  {
    case naming::lower:
      return to_lower<OutputT>(str);
    case naming::upper:
      return to_upper<OutputT>(str);
    case naming::camel_case:
      return to_camel_case<OutputT>(str);
    case naming::snake_case:
      return to_snake_case<OutputT>(str);
    case naming::kebab_case:
      return to_kebab_case<OutputT>(str);
    case naming::pascal_case:
      return to_pascal_case<OutputT>(str);
  }
  return OutputT{str};
}

} // namespace reflex::caseconv