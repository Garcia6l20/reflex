#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/constant.hpp>
#include <reflex/exception.hpp>
#include <reflex/tag_invoke.hpp>
#include <reflex/utils.hpp>

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <charconv>
#include <expected>
#include <format>
#include <ranges>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <optional>
#include <chrono>
#endif

REFLEX_EXPORT namespace reflex
{
  inline constexpr struct __parse_tag : customization_point_object
  {
  } Parse;

  template <typename T>
  concept basic_parsable_c = tag_invocable_c<tag_t<Parse>, std::string_view, std::type_identity<T>>;

  template <typename T>
  concept spec_parsable_c = tag_invocable_c<
      tag_t<Parse>, std::string_view, std::type_identity<T>, constant_wrapper<constant_string{""}>>;

  template <typename T>
  concept parsable_c = basic_parsable_c<T> or spec_parsable_c<T>;

  template <parsable_c T, constant_string Spec> struct __parse_fn : __parse_tag
  {
    constexpr auto operator()(std::string_view s) const
        noexcept(noexcept(tag_invoke(Parse, s, std::type_identity<T>{})))
      requires(basic_parsable_c<T> and not spec_parsable_c<T>)
    {
      return tag_invoke(Parse, s, std::type_identity<T>{});
    }
    constexpr auto operator()(std::string_view s) const
        noexcept(noexcept(tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{})))
      requires(spec_parsable_c<T>)
    {
      return tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{});
    }
    constexpr auto operator()(std::string_view s, constant_wrapper<Spec>) const
        noexcept(noexcept(tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{})))
      requires(spec_parsable_c<T>)
    {
      return tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{});
    }
  };

  template <parsable_c T, constant_string Spec = ""> inline constexpr __parse_fn<T, Spec> parse;

  template <typename T, typename CharT = char> struct parse_result
  {
  private:
    std::optional<T> value_ = std::nullopt;
    const CharT*     ptr_   = nullptr;
    std::errc        error_ = {};

  public:
    constexpr parse_result(T value, const CharT* ptr) : value_(std::move(value)), ptr_(ptr)
    {}
    constexpr parse_result(std::unexpected<std::errc> err) : error_(err.error())
    {}

    template <typename Self> constexpr decltype(auto) value(this Self&& self)
    {
      return std::forward_like<Self>(self.value_).value();
    }
    template <typename Self> constexpr decltype(auto) operator*(this Self&& self)
    {
      return std::forward_like<Self>(self).value();
    }
    constexpr auto error() const -> std::errc
    {
      return error_;
    }
    constexpr auto end() const -> const CharT*
    {
      return ptr_;
    }

    constexpr bool has_value() const noexcept
    {
      return value_.has_value();
    }
    constexpr bool has_error() const noexcept
    {
      return error_ != std::errc{};
    }

    constexpr auto value_or(T&& default_value) const -> T
    {
      if(value_)
        return *value_;
      else
        return std::forward<T>(default_value);
    }

    constexpr auto value_or_throw() const -> T
    {
      if(value_)
        return *value_;
      else
        throw parse_error(
            "Parsing failed: {}", std::generic_category().message(static_cast<int>(error_)));
    }

    constexpr auto value_or_else(auto&& on_error) const -> T
    {
      if(value_)
        return *value_;
      else
        return std::forward<decltype(on_error)>(on_error)(error_);
    }

    constexpr operator bool() const noexcept
    {
      return value_.has_value();
    }
  };

  template <std::integral T>
  constexpr parse_result<T> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<T>) noexcept
  {
    T value{};

    int base = 10;
    if(s.size() > 2 && s[0] == '0')
    {
      if(s[1] == 'x' || s[1] == 'X')
      {
        base = 16;
        s.remove_prefix(2);
      }
      else if(s[1] == 'b' || s[1] == 'B')
      {
        base = 2;
        s.remove_prefix(2);
      }
    }

    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value, base);
    if(ec != std::errc())
    {
      return std::unexpected(ec);
    }
    if(ptr != s.data() + s.size())
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    return {value, ptr};
  }

  template <std::floating_point T>
  constexpr parse_result<T> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<T>) noexcept
  {
    T value{};
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    if(ec != std::errc())
    {
      return std::unexpected(ec);
    }
    if(ptr != s.data() + s.size())
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    return {value, ptr};
  }

  constexpr parse_result<std::string_view> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<std::string_view>) noexcept
  {
    return {s, s.data() + s.size()};
  };

  constexpr parse_result<std::string> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<std::string>) noexcept
  {
    return {std::string(s), s.data() + s.size()};
  };

  constexpr parse_result<bool> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<bool>) noexcept
  {
    using namespace std::string_view_literals;
    static constexpr std::array true_values  = {"true"sv, "yes"sv, "on"sv, "1"sv};
    static constexpr std::array false_values = {"false"sv, "no"sv, "off"sv, "0"sv};

    for(const auto& tv : true_values)
    {
      if(s == tv)
      {
        return {true, s.data() + tv.size()};
      }
    }

    for(const auto& fv : false_values)
    {
      if(s == fv)
      {
        return {false, s.data() + fv.size()};
      }
    }

    return std::unexpected(std::errc::invalid_argument);
  }

  template <typename T>
  concept time_point_c = requires {
    typename T::clock;
    typename T::duration;
  };

  template <constant_string Spec, time_point_c TimePoint>
  constexpr parse_result<TimePoint> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<TimePoint>,
      constant_wrapper<Spec>) noexcept
  {
    std::chrono::year        year{};
    std::chrono::month       month{};
    std::chrono::day         day{};
    std::chrono::hours       hours{};
    std::chrono::minutes     mins{};
    std::chrono::nanoseconds ns{};

    std::string_view spec = Spec;
    while(not spec.empty())
    {
      const auto c = spec.front();
      if(is_space(c))
      {
        spec.remove_prefix(1);
        continue;
      }
      if(c != '%')
      {
        if(s.empty() || s.front() != c)
        {
          return std::unexpected(std::errc::invalid_argument);
        }
        s.remove_prefix(1);
        spec.remove_prefix(1);
        continue;
      }
      switch(spec[1])
      {
        case 'Y':
        {
          const auto parsed = parse<int>(s.substr(0, 4));
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          year = std::chrono::year(parsed.value());
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        case 'm':
        {
          const auto parsed = parse<int>(s.substr(0, 2));
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          month = std::chrono::month(parsed.value());
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        case 'd':
        {
          const auto parsed = parse<int>(s.substr(0, 2));
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          day = std::chrono::day(parsed.value());
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        case 'H':
        {
          const auto parsed = parse<int>(s.substr(0, 2));
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          hours = std::chrono::hours(parsed.value());
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        case 'M':
        {
          const auto parsed = parse<int>(s.substr(0, 2));
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          mins = std::chrono::minutes(parsed.value());
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        case 'S':
        {
          const auto parsed = parse<double>(s);
          if(not parsed.has_value())
          {
            return std::unexpected(parsed.error());
          }
          ns = std::chrono::nanoseconds(static_cast<std::int64_t>(parsed.value() * 1'000'000'000));
          s.remove_prefix(parsed.end() - s.data());
          spec.remove_prefix(2);
          break;
        }
        default:
          if(s.empty() || s.front() != c)
          {
            return std::unexpected(std::errc::invalid_argument);
          }
          s.remove_prefix(1);
          spec.remove_prefix(1);
          break;
      }
    }
    return {std::chrono::sys_days{year / month / day} + hours + mins + ns, s.data()};
  }

  template <parsable_c T, constant_string spec = "">
  constexpr T parse_or(std::string_view s, T fallback)
  {
    return parse<T, spec>(s).value_or(fallback);
  }

  template <parsable_c T, constant_string spec = ""> constexpr T parse_or_throw(std::string_view s)
  {
    return parse<T, spec>(s).value_or_throw();
  }

  template <parsable_c T, constant_string spec = "", typename OnError>
  constexpr T parse_or_else(std::string_view s, OnError && on_error)
  {
    return parse<T, spec>(s).value_or_else(std::forward<OnError>(on_error));
  }
} // namespace reflex
