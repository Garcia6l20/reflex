#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#include <reflex/concepts.hpp>
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
        noexcept(noexcept(_tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{})))
      requires(basic_parsable_c<T> and not spec_parsable_c<T>)
    {
      return _tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{});
    }
    constexpr auto operator()(std::string_view s) const
        noexcept(noexcept(
            _tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{})))
      requires(spec_parsable_c<T>)
    {
      return _tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{});
    }
    constexpr auto operator()(std::string_view s, constant_wrapper<Spec>) const
        noexcept(noexcept(
            _tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{})))
      requires(spec_parsable_c<T>)
    {
      return _tag_invoke_cpo::tag_invoke(Parse, s, std::type_identity<T>{}, constant_wrapper<Spec>{});
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

  /** @brief parsing into an optional yields an engaged one
   *
   * An optional says the value may be absent from the input, not that a value
   * present in the input parses into nothing. Whoever left it out never calls
   * this.
   *
   * The spec is taken and handed to the underlying type, so an optional is
   * transparent to it. Without that, wrapping a type in an optional would drop
   * the spec, and a type reachable only through one, a chrono time_point among
   * them, would never parse.
   */
  template <constant_string Spec, typename T>
  constexpr parse_result<std::optional<T>> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<std::optional<T>>, constant_wrapper<Spec>)
  {
    auto parsed = reflex::parse<T, Spec>(s);
    if(not parsed)
    {
      return std::unexpected{parsed.error()};
    }
    return {std::optional<T>{std::move(parsed).value()}, parsed.end()};
  }

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

  /** @brief a duration written the way a person writes one
   *
   * A number with an optional unit suffix: `100ms`, `1.5s`, `250us`, `2min`.
   * The suffixes are the ones std::chrono itself prints, so a value read here
   * and a value formatted with std::format round trip.
   *
   * A bare number is in the destination's own units, so `parse<milliseconds>`
   * of "100" is 100ms and `parse<seconds>` of "100" is 100s. That is the only
   * rule that stays consistent across destination types, and reading a bare
   * number as seconds regardless would make `parse<nanoseconds>("5")` mean five
   * billion.
   *
   * Negative durations parse. A duration is a signed quantity and refusing one
   * here would be this parser deciding what a caller's domain allows.
   *
   * The arithmetic goes through a double count of nanoseconds and then one
   * duration_cast, so `1.5ms` survives into a `duration<double, std::milli>`
   * and truncates into a `std::chrono::milliseconds` exactly as a
   * duration_cast anywhere else would.
   */
  template <duration_c Duration>
  constexpr parse_result<Duration> tag_invoke(
      tag_t<Parse>, std::string_view s, std::type_identity<Duration>) noexcept
  {
    const auto number = parse<double>(s);
    if(not number.has_value())
    {
      return std::unexpected(number.error());
    }

    std::string_view suffix{number.end(), s.data() + s.size()};

    // Longest match first, so "ms" is never read as "m" followed by rubbish and
    // "min" is never read as "m".
    struct unit
    {
      std::string_view name;
      double           nanoseconds;
    };
    static constexpr std::array units{
        unit{"min", 60.0 * 1e9}, unit{"ns", 1.0},  unit{"us", 1e3},
        unit{"ms", 1e6},         unit{"h", 3600.0 * 1e9}, unit{"s", 1e9},
    };

    using target_period = typename Duration::period;
    // A bare number is in the destination's own units, which is this ratio
    // expressed in nanoseconds.
    double scale = 1e9 * double(target_period::num) / double(target_period::den);
    const char* end = number.end();
    for(unit const& u : units)
    {
      if(suffix.starts_with(u.name))
      {
        scale = u.nanoseconds;
        end += u.name.size();
        break;
      }
    }

    const std::chrono::duration<double, std::nano> nanoseconds{number.value() * scale};
    return {std::chrono::duration_cast<Duration>(nanoseconds), end};
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

  // parse<T> stops as soon as it has a value and reports how far it got through
  // end(), which is what lets a caller read a sequence of fields out of one
  // buffer. parse_strict<T> adds the requirement that nothing is left over.
  //
  //   parse<int>("12abc")        -> 12, end() at 'a'
  //   parse_strict<int>("12abc") -> invalid_argument
  //
  // It wraps parse rather than duplicating any overload, so it applies to every
  // parsable type, including one a user taught through tag_invoke. That is also
  // what an overload owes it: end() has to report what was actually consumed,
  // since claiming the whole input silently defeats the check.
  template <parsable_c T, constant_string spec = "">
  constexpr parse_result<T> parse_strict(std::string_view s)
  {
    auto result = parse<T, spec>(s);
    if(result.has_value() and result.end() != s.data() + s.size())
    {
      return std::unexpected(std::errc::invalid_argument);
    }
    return result;
  }

  template <parsable_c T, constant_string spec = "">
  constexpr T parse_or(std::string_view s, T fallback)
  {
    return parse<T, spec>(s).value_or(std::move(fallback));
  }

  template <parsable_c T, constant_string spec = "">
  constexpr T parse_strict_or(std::string_view s, T fallback)
  {
    return parse_strict<T, spec>(s).value_or(std::move(fallback));
  }

  template <parsable_c T, constant_string spec = ""> constexpr T parse_or_throw(std::string_view s)
  {
    return parse<T, spec>(s).value_or_throw();
  }

  template <parsable_c T, constant_string spec = "">
  constexpr T parse_strict_or_throw(std::string_view s)
  {
    return parse_strict<T, spec>(s).value_or_throw();
  }

  template <parsable_c T, constant_string spec = "", typename OnError>
  constexpr T parse_or_else(std::string_view s, OnError && on_error)
  {
    return parse<T, spec>(s).value_or_else(std::forward<OnError>(on_error));
  }

  template <parsable_c T, constant_string spec = "", typename OnError>
  constexpr T parse_strict_or_else(std::string_view s, OnError && on_error)
  {
    return parse_strict<T, spec>(s).value_or_else(std::forward<OnError>(on_error));
  }
} // namespace reflex
