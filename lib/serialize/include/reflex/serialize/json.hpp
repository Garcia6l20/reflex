#pragma once

#include <reflex/meta.hpp>
#include <reflex/unicode.hpp>
#include <reflex/utility.hpp>

#if __has_include(<reflex/var.hpp>)
#include <reflex/var.hpp>
#endif

#include <charconv>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>

namespace reflex::serialize::json
{
#if __has_include(<reflex/var.hpp>)

using string  = std::string;
using number  = double;
using boolean = bool;
using value   = recursive_var<boolean, number, string>;
using array   = typename value::vec_t;
using object  = typename value::map_t;
#define REFLEX_JSON_POLY_SUPPORT (true)

constexpr value parse(auto& begin, auto const& end);
#else
#define REFLEX_JSON_POLY_SUPPORT (false)
#endif

class error : public std::runtime_error
{
public:
  error(std::string_view what) noexcept : std::runtime_error{what.data()}
  {
  }
};

constexpr auto it_find(auto const& begin, auto const& end, char c)
{
  auto ii = begin;
  for(; ii != end && *ii != c; ++ii)
  {
  }
  return ii;
}

template <meta::info R>
  requires(dealias(R) == dealias(^^std::string))
constexpr std::string load(auto& begin, auto const& end)
{
  std::string result;
  while(*begin != '"' and begin != end)
  {
    ++begin;
  }
  if(begin == end)
  {
    return {};
  }
  ++begin;
  for(; begin != end; ++begin)
  {
    switch(*begin)
    {
      case '"':
        ++begin;
        return result;
      case '\\':
      {
        // handle escape char
        switch(*++begin)
        {
          case 'n':
            result += '\n';
            break;
          case '\\':
            result += '\\';
            break;
          case '/':
            result += '/';
            break;
          case 'b':
            result += '\b';
            break;
          case 'f':
            result += '\f';
            break;
          case 'r':
            result += '\r';
            break;
          case 't':
            result += '\t';
            break;
          case 'u':
          {
            uint16_t code;
            auto [ptr, ec] = std::from_chars(&*(begin + 1), &*(begin + 5), code, 16);
            if(ec != std::errc{})
            {
              throw error{format("failed to parse unicode: {}", std::make_error_code(ec).message())};
            }
            begin    = ptr - 1;
            auto out = std::back_inserter(result);
            if(!unicode::to_utf8(out, code))
            {
              throw error{"invalid unicode charater"};
            }
          }
          break;
          default:
            throw error(std::format("Unknown escape character: {}", *begin));
        }
        break;
      }
      default:
        if(is_print(*begin))
          result += *begin;
    }
  }
  throw std::runtime_error("Error occurred while parsing string");
}

template <meta::info R>
  requires(is_arithmetic_type(R) and R != ^^bool)
constexpr auto load(auto& begin, auto const& end)
{
  double result = 0.;
  // trim
  while(not is_digit(*begin) and begin != end)
  {
    ++begin;
  }
  auto [ptr, ec] = std::from_chars(&*begin, &*end, result);
  if(ec != std::errc{})
  {
    throw error{format("failed to parse number: {}", std::make_error_code(ec).message())};
  }
  begin = ptr;
  return result;
}

template <meta::info R>
  requires(R == ^^bool)
constexpr auto load(auto& begin, auto const& end)
{
  while(not is_alpha(*begin) and begin != end)
  {
    ++begin;
  }
  if(std::string_view{begin, end}.starts_with("true"))
  {
    begin += 4;
    return true;
  }
  else if(std::string_view{begin, end}.starts_with("false"))
  {
    begin += 5;
    return false;
  }
  throw error(std::format("unexpected boolean expression"));
}

consteval bool is_loadable_object(meta::info R)
{
  auto D = dealias(R);
  return is_aggregate_type(D)
#if REFLEX_JSON_POLY_SUPPORT
         or D == dealias(^^object)
#endif
      ;
}

consteval bool is_loadable_array(meta::info R)
{
  return meta::is_template_instance_of(dealias(R), ^^std::vector);
}

template <meta::info R>
  requires(is_loadable_object(R))
constexpr auto load(auto& begin, auto const& end);

template <meta::info R>
  requires(is_loadable_array(R))
constexpr auto load(auto& begin, auto const& end)
{
  using T = [:dealias(R):];
  T result{};
  if(*begin != '[')
  {
    begin = it_find(begin, end, '[') + 1;
  }
  else
  {
    ++begin;
  }
  while(*begin != ']' and begin != end)
  {
    if(!is_graph(*begin))
    {
      ++begin;
      continue;
    }
#if REFLEX_JSON_POLY_SUPPORT
    if constexpr(dealias(^^typename T::value_type) == dealias(^^value))
    {
      result.push_back(parse(begin, end));
    }
    else
    {
#endif // REFLEX_JSON_POLY_SUPPORT
      result.push_back(load<^^typename T::value_type>(begin, end));
#if REFLEX_JSON_POLY_SUPPORT
    }
#endif // REFLEX_JSON_POLY_SUPPORT
  }
  return result;
}

template <meta::info R>
  requires(is_loadable_object(R))
constexpr auto load(auto& begin, auto const& end)
{
  constexpr auto members =
      define_static_array(meta::nonstatic_data_members_of_r(R, std::meta::access_context::current()));
  using T = [:R:];
  T              result{};
  constexpr auto identifier = identifier_of(R);
  if(*begin != '{')
  {
    begin = it_find(begin, end, '{') + 1;
  }
  else
  {
    ++begin;
  }
  while(*begin != '}' and begin != end)
  {
    if(!is_graph(*begin))
    {
      ++begin;
      continue;
    }
    const auto key = load<^^std::string>(begin, end);
    begin          = it_find(begin, end, ':');
    if(begin == end)
    {
      throw error("syntax error");
    }
    ++begin;

#if REFLEX_JSON_POLY_SUPPORT
    if constexpr(dealias(^^T) == dealias(^^object))
    {
      result[key] = parse(begin, end);
    }
    else
    {
#endif // REFLEX_JSON_POLY_SUPPORT
      bool found = false;
      template for(constexpr auto field : members)
      {
        if(identifier_of(field) == key)
        {
          result.[:field:] = load<type_of(field)>(begin, end);
          found            = true;
          break;
        }
      }
      if(not found)
      {
        throw error(std::format("field {} not found in {}", key, identifier));
      }
#if REFLEX_JSON_POLY_SUPPORT
    }
#endif // REFLEX_JSON_POLY_SUPPORT
  }
  ++begin;

  return result;
}

#if __has_include(<reflex/var.hpp>)
constexpr value parse(auto& begin, auto const& end)
{
  value result;
  for(; begin != end; ++begin)
  {
    switch(*begin)
    {
      case ' ':
      case '\n':
      case '\t':
        continue;
      case 't': // assuming true
        begin += 4;
        result = true;
        return result;
      case 'f': // assuming false
        begin += 5;
        result = false;
        return result;
      case 'n': // assuming null
        begin += 4;
        return result;
      case '{':
        result = load<^^object>(begin, end);
        return result;
        break;
      case '[':
        result = load<^^array>(begin, end);
        return result;
        break;
      case '"':
        result = load<^^string>(begin, end);
        return result;
      default:
        result = load<^^number>(begin, end);
        return result;
    }
  }
  throw std::runtime_error("Error occurred while parsing value");
}

constexpr auto load(std::string_view body) -> value
{
  auto begin = std::begin(body);
  return parse(begin, std::end(body));
}
#endif

template <typename T> constexpr auto load(std::string_view body) -> T
{
  auto begin = std::begin(body);
  return load<^^T>(begin, std::end(body));
}

template <typename Out, typename T>
  requires std::output_iterator<Out, const char&>
constexpr Out dump_to(Out out, T const& value)
{
  constexpr auto type    = decay(^^T);
  constexpr auto da_type = dealias(type);
  if constexpr(type == ^^bool)
  {
    return std::format_to(out, "{}", value ? "true" : "false");
  }
  else if constexpr(is_arithmetic_type(type))
  {
    return std::format_to(out, "{}", value);
  }
  else if constexpr(da_type == dealias(^^std::string) or da_type == dealias(^^std::string_view) or da_type == ^^char*)
  {
    return std::format_to(out, "\"{}\"", value);
  }
  else if constexpr(is_aggregate_type(da_type))
  {
    constexpr auto members =
        define_static_array(meta::nonstatic_data_members_of_r(type, std::meta::access_context::current()));
    out                         = '{';
    constexpr auto member_count = members.size();
    template for(constexpr auto ii : std::views::iota(size_t(0), member_count))
    {
      constexpr auto member = members[ii];
      out                   = std::format_to(out, "\"{}\": ", identifier_of(member));
      out                   = dump_to(out, value.[:member:]);
      if(ii != member_count - 1)
      {
        out = ',';
      }
    }
    out = '}';
    return out;
  }
  else if constexpr(std::ranges::range<typename[:da_type:]>)
  {
    const auto item_count = std::ranges::size(value);
    out                   = '[';
    for(const auto ii : std::views::iota(size_t(0), item_count))
    {
      out = dump_to(out, value.at(ii));
      if(ii != item_count - 1)
      {
        out = ',';
      }
    }
    out = ']';
    return out;
  }
  else
  {
    std::unreachable();
  }
}

template <typename T> constexpr auto dump(T const& value) -> std::string
{
  std::string result;
  dump_to(std::back_inserter(result), value);
  return result;
}

} // namespace reflex::serialize::json
