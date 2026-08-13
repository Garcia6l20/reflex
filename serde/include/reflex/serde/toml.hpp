#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cmath>
#include <cstring>

#include <reflex/format.hpp>
#include <reflex/heapless/string.hpp>
#include <reflex/parse.hpp>
#include <reflex/serde.hpp>
#include <reflex/serde/toml_value.hpp>
#endif

#include <reflex/serde/detail/escape.hpp>
#include <reflex/serde/detail/io.hpp>
#include <reflex/serde/detail/line_cursor.hpp>
#include <reflex/serde/detail/text.hpp>

REFLEX_EXPORT namespace reflex::serde::toml
{
  namespace detail
  {
  using serde::detail::string_view_of;

  using escapes = serde::detail::toml_escapes;

  constexpr bool needs_escape(char c)
  {
    return serde::detail::needs_escape<escapes, '"'>(c);
  }

  // A literal string ('...') cannot carry a single quote and bars every control
  // character but tab. Worth choosing only where it saves an escape: 'C:\path\to'
  // beats "C:\\path\\to". A tab is left to the basic form, where it shows as \t.
  constexpr bool prefers_literal(std::string_view text)
  {
    bool worth_it = false;
    for(char c : text)
    {
      if(c == '\'' or serde::detail::is_control(c))
      {
        return false;
      }
      if(c == '\\' or c == '"')
      {
        worth_it = true;
      }
    }
    return worth_it;
  }

  // Multi-line forms are never emitted: a newline goes out as \n in a basic
  // string, which is shorter and free of the line-continuation rules.
  template <typename Ser> void write_string(Ser& ser, std::string_view text)
  {
    if(prefers_literal(text))
    {
      // Not write_doubled_quoted: a literal string cannot contain its quote at all.
      ser.write_char('\'');
      ser.write_raw(text);
      ser.write_char('\'');
      return;
    }
    serde::detail::write_backslash_quoted<escapes, '"'>(ser, text);
  }

  constexpr bool is_bare_key_char(char c)
  {
    return serde::detail::is_dec_digit(c) or (c >= 'a' and c <= 'z') or (c >= 'A' and c <= 'Z')
        or c == '_' or c == '-';
  }

  constexpr bool is_bare_key(std::string_view s)
  {
    return not s.empty() and std::ranges::all_of(s, is_bare_key_char);
  }

  template <typename Ser> void write_key(Ser& ser, std::string_view name)
  {
    if(is_bare_key(name))
    {
      ser.write_raw(name);
      return;
    }
    serde::detail::write_backslash_quoted<escapes, '"'>(ser, name);
  }

  // Checked rather than escaped: serde::rename already bars a dot, a quote, a
  // backslash and control characters (annotations.hpp:19). The barred dot is what
  // lets a name be spliced into a [a.b.c] path with no ambiguity about where one
  // segment ends.
  template <std::meta::info Member> consteval std::string_view key_name()
  {
    constexpr std::string_view name = serialized_name(Member);
    static_assert(
        std::ranges::none_of(name, needs_escape),
        "a TOML key cannot contain a quote, a backslash or a control character: "
        "check the serde::rename annotation on this member");
    if constexpr(is_bare_key(name))
    {
      return {std::define_static_string(name), name.size()};
    }
    else
    {
      std::string s;
      s.reserve(name.size() + 2);
      s += '"';
      s += name;
      s += '"';
      return {std::define_static_string(s), s.size()};
    }
  }

  // "name = ", built once at compile time and promoted to static storage.
  template <std::meta::info Member> consteval std::string_view assign_key()
  {
    std::string s{key_name<Member>()};
    s += " = ";
    return {std::define_static_string(s), s.size()};
  }

  // TOML's Float needs a fractional or an exponent part, so a bare `1` is an
  // Integer. to_chars is shortest-round-trip and gives "1" for the double 1.0,
  // hence the ".0". inf and nan bypass it so the ".0" test never sees them.
  template <typename Ser, std::floating_point Num> void write_float(Ser& ser, Num value)
  {
    if(std::isnan(value))
    {
      ser.write_raw("nan");
      return;
    }
    if(not std::isfinite(value))
    {
      ser.write_raw(value < 0 ? "-inf" : "inf");
      return;
    }

    char       buf[64];
    const auto r    = std::to_chars(buf, buf + sizeof(buf), value);
    const auto text = std::string_view{buf, static_cast<std::size_t>(r.ptr - buf)};
    ser.write_raw(text);
    if(text.find_first_of(".eE") == std::string_view::npos)
    {
      ser.write_raw(".0");
    }
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "TOML";

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, null_t const&)
    {
      throw std::runtime_error(
          "TOML has no null: a null value can only be omitted with its key, and inside an array or "
          "an inline table there is no key to omit");
      return ser.out();
    }

    // An empty optional never reaches here from a table member - the table
    // writer omits the key. It reaches here from an array element or an inline
    // table value, where TOML gives it nowhere to go: `x = [1, , 3]` does not
    // exist.
    template <typename T>
    friend OutputIt tag_invoke(
        tag_t<serde::serialize>,
        serializer<OutputIt>&   ser,
        std::optional<T> const& value)
    {
      if(not value.has_value())
      {
        throw std::runtime_error(
            "TOML has no null: an empty optional can only be omitted with its key, and inside an "
            "array or an inline table there is no key to omit");
      }
      serialize(ser, *value);
      return ser.out();
    }

    template <str_c Str>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Str const& value)
    {
      detail::write_string(ser, detail::string_view_of(value));
      return ser.out();
    }

    template <number_c Num>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Num const& value)
    {
      if constexpr(std::floating_point<Num>)
      {
        detail::write_float(ser, value);
      }
      else
      {
        serde::detail::write_digits(ser, value);
      }
      return ser.out();
    }

    template <std::same_as<char> Char>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Char value)
    {
      detail::write_string(ser, std::string_view{&value, 1});
      return ser.out();
    }

    template <std::same_as<boolean> Boolean>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Boolean value)
    {
      ser.write_raw(value ? "true" : "false");
      return ser.out();
    }

    template <derives_c<derive_t<Format>> T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      // Scanned by write_string before it reaches the sink, so it cannot be
      // formatted straight through.
      std::array<char, 128> buf{};
      const auto            r = std::format_to_n(buf.data(), buf.size(), "{}", value);
      if(static_cast<std::size_t>(r.size) <= buf.size())
      {
        detail::write_string(ser, std::string_view{buf.data(), static_cast<std::size_t>(r.size)});
      }
      else
      {
        detail::write_string(ser, std::format("{}", value));
      }
      return ser.out();
    }
  };

  template <typename... TArgs>
  serializer(std::basic_string<TArgs...> & out)
      -> serializer<std::back_insert_iterator<std::basic_string<TArgs...>>>;
  serializer(std::ofstream & out) -> serializer<std::ostreambuf_iterator<char>>;
  serializer(std::ostringstream & out) -> serializer<std::ostreambuf_iterator<char>>;

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::line_cursor<InputIt>
  {
  public:
    using serde::detail::line_cursor<InputIt>::line_cursor;

    static constexpr std::string_view format_name = "TOML";

    // makes load() without an explicit type read a toml::value
    using default_load_type = toml::value;
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

} // namespace reflex::serde::toml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto toml = ^^reflex::serde::toml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto toml = ^^reflex::serde::toml::deserializer;
}
