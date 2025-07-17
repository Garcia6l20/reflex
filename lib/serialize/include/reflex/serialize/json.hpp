#pragma once

#include <reflex/meta.hpp>
#include <reflex/unicode.hpp>

#include <charconv>
#include <format>
#include <iterator>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>

namespace reflex::serialize::json
{

namespace detail
{
constexpr bool is_in_range(int c, int low, int high)
{
  return (c >= low) and (c <= high);
}
constexpr bool is_cntrl(int c) noexcept
{
  return is_in_range(c, '\x00', '\x1f') //
         or c == '\x7f';
}
constexpr bool is_print(int c) noexcept
{
  return is_in_range(c, '\x20', '\x7e');
}
constexpr bool is_graph(int c) noexcept
{
  return is_in_range(c, '\x21', '\x7e');
}
constexpr bool is_blank(int c) noexcept
{
  return c == '\x09' or c == '\x20';
}
constexpr bool is_space(int c) noexcept
{
  return is_in_range(c, '\x09', '\x0d') //
         or c == '\x20';
}
constexpr bool is_upper(int c) noexcept
{
  return is_in_range(c, '\x41', '\x5a');
}
constexpr bool is_alpha(int c) noexcept
{
  return is_upper(c) //
         or is_in_range(c, '\x61', '\x7a');
}
constexpr bool is_digit(int c) noexcept
{
  return is_in_range(c, '\x30', '\x39');
}
constexpr bool is_xdigit(int c) noexcept
{
  return is_digit(c)                       //
         or is_in_range(c, '\x41', '\x46') //
         or is_in_range(c, '\x61', '\x66');
}
constexpr bool is_alphanum(int c) noexcept
{
  return is_digit(c) or is_alpha(c);
}
constexpr bool is_punct(int c) noexcept
{
  return is_in_range(c, '\x21', '\x2f')    //
         or is_in_range(c, '\x3a', '\x40') //
         or is_in_range(c, '\x5b', '\x60') //
         or is_in_range(c, '\x7b', '\x7e');
}
} // namespace detail

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
        if(detail::is_print(*begin))
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
  while(not detail::is_digit(*begin) and begin != end)
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
  while(not detail::is_alpha(*begin) and begin != end)
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

template <meta::info R>
  requires(is_aggregate_type(R))
constexpr auto load(auto& begin, auto const& end);

template <meta::info R>
  requires(has_template_arguments(R) and template_of(R) == ^^std::vector)
constexpr auto load(auto& begin, auto const& end)
{
  using T = [:R:];
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
    if(!detail::is_graph(*begin))
    {
      ++begin;
      continue;
    }
    result.push_back(load<^^typename T::value_type>(begin, end));
  }
  return result;
}

template <meta::info R>
  requires(is_aggregate_type(R))
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
    if(!detail::is_graph(*begin))
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
  }
  ++begin;

  return result;
}

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
