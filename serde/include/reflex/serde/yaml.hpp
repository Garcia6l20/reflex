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
#include <reflex/serde/yaml_value.hpp>
#endif

#include <reflex/serde/detail/io.hpp>

REFLEX_EXPORT namespace reflex::serde::yaml
{
  namespace detail
  {
  constexpr bool is_dec_digit(char c)
  {
    return c >= '0' and c <= '9';
  }

  constexpr bool is_hex_digit(char c)
  {
    return is_dec_digit(c) or (c >= 'a' and c <= 'f') or (c >= 'A' and c <= 'F');
  }

  // A byte that cannot appear literally in a double-quoted scalar. Bytes 0x80 and
  // above are absent: they are UTF-8 lead and continuation bytes and pass through
  // unchanged.
  constexpr bool is_control(char c)
  {
    const auto u = static_cast<unsigned char>(c);
    return u < 0x20 or u == 0x7F;
  }

  constexpr char lower(char c)
  {
    return (c >= 'A' and c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  }

  constexpr bool iequals(std::string_view a, std::string_view b)
  {
    return a.size() == b.size()
       and std::ranges::equal(a, b, [](char x, char y) { return lower(x) == lower(y); });
  }

  constexpr bool iequals_any(std::string_view s, std::initializer_list<std::string_view> set)
  {
    return std::ranges::any_of(set, [s](std::string_view c) { return iequals(s, c); });
  }

  // A plain scalar may not begin with one of these. '-', '?' and ':' are absent:
  // they only bar a plain scalar when a space or the end of the scalar follows,
  // and that is tested separately.
  constexpr bool is_indicator(char c)
  {
    return c == ',' or c == '[' or c == ']' or c == '{' or c == '}' or c == '#' or c == '&'
        or c == '*' or c == '!' or c == '|' or c == '>' or c == '\'' or c == '"' or c == '%'
        or c == '@' or c == '`';
  }

  // The YAML 1.2 core schema's integer forms: [-+]?[0-9]+, 0o[0-7]+, 0x[0-9a-fA-F]+.
  constexpr bool matches_int(std::string_view s)
  {
    if(s.starts_with("0o"))
    {
      const auto body = s.substr(2);
      return not body.empty()
         and std::ranges::all_of(body, [](char c) { return c >= '0' and c <= '7'; });
    }
    if(s.starts_with("0x"))
    {
      const auto body = s.substr(2);
      return not body.empty() and std::ranges::all_of(body, is_hex_digit);
    }
    if(s.starts_with('-') or s.starts_with('+'))
    {
      s.remove_prefix(1);
    }
    return not s.empty() and std::ranges::all_of(s, is_dec_digit);
  }

  // [-+]? ( \.[0-9]+ | [0-9]+ (\.[0-9]*)? ) ( [eE][-+]?[0-9]+ )?, plus the
  // non-finite spellings.
  //
  // Hand-rolled rather than probed with std::from_chars because this has to run
  // at compile time for plain_key(), and the floating-point overload of
  // from_chars is not constexpr. Doing it by hand also keeps the accepted set
  // exactly YAML's rather than whatever from_chars happens to take - it would
  // accept "0x1p3" and "infinity", neither of which YAML resolves as a number.
  constexpr bool matches_float(std::string_view s)
  {
    if(iequals_any(s, {".inf", "-.inf", "+.inf", ".nan"}))
    {
      return true;
    }
    if(s.starts_with('-') or s.starts_with('+'))
    {
      s.remove_prefix(1);
    }

    std::size_t i      = 0;
    bool        digits = false;
    while(i < s.size() and is_dec_digit(s[i]))
    {
      ++i;
      digits = true;
    }
    if(i < s.size() and s[i] == '.')
    {
      ++i;
      while(i < s.size() and is_dec_digit(s[i]))
      {
        ++i;
        digits = true;
      }
    }
    if(not digits)
    {
      return false;
    }
    if(i < s.size() and (s[i] == 'e' or s[i] == 'E'))
    {
      ++i;
      if(i < s.size() and (s[i] == '-' or s[i] == '+'))
      {
        ++i;
      }
      const std::size_t exp_start = i;
      while(i < s.size() and is_dec_digit(s[i]))
      {
        ++i;
      }
      if(i == exp_start)
      {
        return false;
      }
    }
    return i == s.size();
  }

  // Would this text, written plain, read back as something other than a string?
  //
  // The 1.1 boolean spellings are in here deliberately. YAML 1.2 resolves them as
  // strings, so this library would round-trip them either way, but a 1.1 reader
  // would not - and a document that means two different things in two readers is
  // a bug even when we are self-consistent about it.
  //
  // Case-insensitive throughout, which is wider than the core schema's six
  // boolean spellings. Over-quoting is safe; under-quoting is not.
  constexpr bool resolves_as_non_string(std::string_view s)
  {
    if(s.empty())
    {
      return true; // an empty plain scalar is null
    }
    if(s == "~" or iequals(s, "null"))
    {
      return true;
    }
    if(iequals_any(s, {"true", "false"}))
    {
      return true;
    }
    if(iequals_any(s, {"yes", "no", "on", "off", "y", "n"}))
    {
      return true; // YAML 1.1 booleans
    }
    if(s.starts_with("---") or s.starts_with("..."))
    {
      return true; // document markers
    }
    return matches_int(s) or matches_float(s);
  }

  // Whether `s` can be written without quotes in block context.
  //
  // Block context only. A flow collection additionally bars ',', '[', ']', '{'
  // and '}', but the serializer only ever emits flow style for an empty
  // collection, where there is no scalar to quote. Do not widen this set without
  // also giving the caller a way to say which context it is in.
  constexpr bool needs_quoting(std::string_view s)
  {
    if(resolves_as_non_string(s))
    {
      return true;
    }
    if(s.front() == ' ' or s.back() == ' ')
    {
      return true;
    }
    if(is_indicator(s.front()))
    {
      return true;
    }
    // '-', '?' and ':' only bar a plain scalar when the scalar is exactly that
    // character or a space follows it.
    if(s.front() == '-' or s.front() == '?' or s.front() == ':')
    {
      if(s.size() == 1 or s[1] == ' ')
      {
        return true;
      }
    }
    if(s.back() == ':')
    {
      return true;
    }
    for(std::size_t i = 0; i + 1 < s.size(); ++i)
    {
      if(s[i] == ':' and s[i + 1] == ' ')
      {
        return true;
      }
      if(s[i] == ' ' and s[i + 1] == '#')
      {
        return true;
      }
    }
    return std::ranges::any_of(s, [](char c) { return is_control(c) or c == '\t'; });
  }

  // Quote by doubling: no backslash is special, the quote character escapes
  // itself. csv.hpp's RFC 4180 cell writer is this function with Quote = '"'.
  //
  // One needle over the remainder, so the doubling loop cannot restart from the
  // front and go quadratic.
  template <char Quote, typename Ser> void write_doubled_quoted(Ser& ser, std::string_view text)
  {
    ser.write_char(Quote);
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const std::size_t n = text.find(Quote, pos);
      if(n == std::string_view::npos)
      {
        ser.write_raw(text.substr(pos));
        break;
      }
      ser.write_raw(text.substr(pos, n - pos + 1)); // the run, quote included
      ser.write_char(Quote);                        // the doubling
      pos = n + 1;
    }
    ser.write_char(Quote);
  }

  // The two-character escapes YAML names, or '\0' when the byte has none and
  // needs the \xXX form.
  constexpr char simple_escape(char c)
  {
    switch(c)
    {
      case '\0':
        return '0';
      case '\a':
        return 'a';
      case '\b':
        return 'b';
      case '\t':
        return 't';
      case '\n':
        return 'n';
      case '\v':
        return 'v';
      case '\f':
        return 'f';
      case '\r':
        return 'r';
      case '\x1B':
        return 'e';
      default:
        return '\0';
    }
  }

  template <char Quote, typename Ser> void write_escape(Ser& ser, char c)
  {
    if(c == Quote or c == '\\')
    {
      ser.write_char('\\');
      ser.write_char(c);
      return;
    }
    if(const char esc = simple_escape(c); esc != '\0')
    {
      ser.write_char('\\');
      ser.write_char(esc);
      return;
    }
    static constexpr std::string_view hex = "0123456789abcdef";
    const auto                        u   = static_cast<unsigned char>(c);
    const char                        buf[4]{'\\', 'x', hex[u >> 4], hex[u & 0x0F]};
    ser.write_raw(std::string_view{buf, sizeof(buf)});
  }

  // Backslash-escaping writer, quote character parameterised. Runs of clean
  // bytes go out whole; only a byte needing an escape is handled alone.
  //
  // json.hpp:121-143 is this function with Quote = '"' and a JSON escape table,
  // and csv.hpp:84-113 is write_doubled_quoted above. Both are candidates for a
  // shared home in detail/io.hpp; kept local until that is proposed with a diff.
  template <char Quote, typename Ser> void write_backslash_quoted(Ser& ser, std::string_view text)
  {
    ser.write_char(Quote);
    std::size_t pos = 0;
    while(pos < text.size())
    {
      const auto* const first = text.data() + pos;
      const auto* const last  = text.data() + text.size();
      const auto*       it =
          std::find_if(first, last, [](char c) { return is_control(c) or c == Quote or c == '\\'; });
      if(it == last)
      {
        ser.write_raw(text.substr(pos));
        break;
      }
      const auto n = static_cast<std::size_t>(it - first);
      ser.write_raw(text.substr(pos, n));
      write_escape<Quote>(ser, *it);
      pos += n + 1;
    }
    ser.write_char(Quote);
  }

  // Plain when it is safe, single-quoted when it can be, double-quoted only when
  // there is no other choice. Single-quoted holds any printable byte, so the
  // double-quoted form is reached exactly when a control character or a tab is
  // present.
  template <typename Ser> void write_scalar(Ser& ser, std::string_view text)
  {
    if(not needs_quoting(text))
    {
      ser.write_raw(text);
      return;
    }
    if(std::ranges::none_of(text, [](char c) { return is_control(c) or c == '\t'; }))
    {
      write_doubled_quoted<'\''>(ser, text);
      return;
    }
    write_backslash_quoted<'"'>(ser, text);
  }

  // A std::array<char, N> is a fixed buffer, trimmed at the first NUL.
  template <typename Str> std::string_view string_view_of(Str const& value)
  {
    if constexpr(meta::is_template_instance_of(^^Str, ^^std::array))
    {
      return std::string_view{value.data(), ::strnlen(value.data(), value.size())};
    }
    else
    {
      return std::string_view{value};
    }
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "YAML";

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, null_t const&)
    {
      ser.write_raw("null");
      return ser.out();
    }

    template <typename T>
    friend OutputIt tag_invoke(
        tag_t<serde::serialize>,
        serializer<OutputIt>&   ser,
        std::optional<T> const& value)
    {
      if(value.has_value())
      {
        serialize(ser, *value);
      }
      else
      {
        ser.write_raw("null");
      }
      return ser.out();
    }

    template <str_c Str>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Str const& value)
    {
      detail::write_scalar(ser, detail::string_view_of(value));
      return ser.out();
    }

    template <number_c Num>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Num const& value)
    {
      // Unlike JSON, YAML has a literal for every non-finite value. It has to be
      // written here rather than left to write_digits, which would emit "inf"
      // and "nan" - spellings a YAML reader resolves as strings, not numbers.
      if constexpr(std::floating_point<Num>)
      {
        if(std::isnan(value))
        {
          ser.write_raw(".nan");
          return ser.out();
        }
        if(not std::isfinite(value))
        {
          ser.write_raw(value < 0 ? "-.inf" : ".inf");
          return ser.out();
        }
      }
      serde::detail::write_digits(ser, value);
      return ser.out();
    }

    template <std::same_as<char> Char>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Char value)
    {
      detail::write_scalar(ser, std::string_view{&value, 1});
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
      // The rendered text has to be scanned by write_scalar before it reaches the
      // sink, so it cannot be formatted straight through. Anything a formatter is
      // likely to produce here fits the stack buffer.
      std::array<char, 128> buf{};
      const auto            r = std::format_to_n(buf.data(), buf.size(), "{}", value);
      if(static_cast<std::size_t>(r.size) <= buf.size())
      {
        detail::write_scalar(ser, std::string_view{buf.data(), static_cast<std::size_t>(r.size)});
      }
      else
      {
        detail::write_scalar(ser, std::format("{}", value));
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
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

  public:
    using base::at_end;
    using base::base;

    // makes load() without an explicit type read a yaml::value
    using default_load_type = yaml::value;
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

} // namespace reflex::serde::yaml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto yaml = ^^reflex::serde::yaml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto yaml = ^^reflex::serde::yaml::deserializer;
}
