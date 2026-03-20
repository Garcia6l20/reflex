#pragma once

#include <reflex/utils.hpp>

namespace reflex::caseconv
{
constexpr std::string to_snake_case(std::string_view str)
{
  std::string result;
  result.reserve(str.size());
  for(std::size_t i = 0; i < str.size(); ++i)
  {
    char c = str[i];
    if(is_upper(c))
    {
      if(i > 0 && is_lower(str[i - 1]))
      {
        result += '_';
      }
      result += to_lower(c);
    }
    else
    {
      result += c;
    }
  }
  return result;
}
constexpr std::string to_pascal_case(std::string_view str)
{
  std::string result;
  result.reserve(str.size());
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
constexpr std::string to_kebab_case(std::string_view str)
{
  std::string result = to_snake_case(str);
  for(char& c : result)
  {
    if(c == '_')
    {
      c = '-';
    }
  }
  return result;
}
constexpr std::string to_camel_case(std::string_view str)
{
  std::string result = to_pascal_case(str);
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

constexpr std::string to_case(std::string_view str, naming n)
{
  switch(n)
  {
    case naming::camel_case:
      return to_camel_case(str);
    case naming::snake_case:
      return to_snake_case(str);
    case naming::kebab_case:
      return to_kebab_case(str);
    case naming::pascal_case:
      return to_pascal_case(str);
  }
  return std::string{str};
}

} // namespace reflex::caseconv