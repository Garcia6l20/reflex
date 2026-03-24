#pragma once

#include <reflex/utils.hpp>

namespace reflex::caseconv
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
      result += to_lower(c);
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
        result += to_upper(c);
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
  result[0] = to_lower(result[0]);
  return result;
}

enum class naming
{
  camel_case,
  snake_case,
  kebab_case,
  pascal_case
};

template <typename OutputT = std::string> constexpr OutputT to_case(std::string_view str, naming n)
{
  switch(n)
  {
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