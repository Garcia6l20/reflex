#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string>
#include <string_view>
#endif

#include <reflex/serde/detail/io.hpp>
#include <reflex/serde/detail/text.hpp>

REFLEX_EXPORT namespace reflex::serde::detail
{
  // Quote by doubling: no backslash is special, the quote character escapes
  // itself. One needle over the remainder, so the doubling loop cannot restart
  // from the front and go quadratic.
  template <char Quote, typename Ser> void write_doubled_quoted(Ser& ser, std::string_view text)
  {
    static constexpr char doubled[3]{Quote, Quote, '\0'};

    ser.write_char(Quote);
    serde::detail::write_with_escapes(
        ser,
        text,
        [](std::string_view s, std::size_t pos) { return s.find(Quote, pos); },
        [](Ser& out, char) { out.write_raw(doubled); });
    ser.write_char(Quote);
  }

  template <typename Ser> void write_hex2_escape(Ser& ser, char c)
  {
    static constexpr std::string_view hex = "0123456789abcdef";
    const auto                        u   = static_cast<unsigned char>(c);
    const char                        buf[4]{'\\', 'x', hex[u >> 4], hex[u & 0x0F]};
    ser.write_raw(std::string_view{buf, sizeof(buf)});
  }

  // JSON's only fallback: it has no \x form.
  template <typename Ser> void write_u00_escape(Ser& ser, char c)
  {
    static constexpr std::string_view hex = "0123456789abcdef";
    const auto                        u   = static_cast<unsigned char>(c);
    const char                        buf[6]{'\\', 'u', '0', '0', hex[u >> 4], hex[u & 0x0F]};
    ser.write_raw(std::string_view{buf, sizeof(buf)});
  }

  // How a format spells an escape, one of these per backend. A trait rather than
  // template parameters, because the fallback form differs in shape (\xXX against
  // \u00XX) rather than in content; the quote character stays a parameter of the
  // writer, since YAML varies it within itself.
  struct json_escapes
  {
    // 0x00-0x1F only. 0x7F is a literal byte inside a JSON string, which is why
    // this is not detail::is_control.
    static constexpr bool is_control_byte(char c)
    {
      return static_cast<unsigned char>(c) < 0x20;
    }

    // '"' and '\\' are absent: the writer handles the active quote and the
    // backslash before it asks. '/' is absent because JSON allows escaping it
    // but does not require it, and the decoder still accepts it.
    static constexpr char two_char(char c)
    {
      switch(c)
      {
        case '\b':
          return 'b';
        case '\f':
          return 'f';
        case '\n':
          return 'n';
        case '\r':
          return 'r';
        case '\t':
          return 't';
        default:
          return '\0';
      }
    }

    template <typename Ser> static void write_fallback(Ser& ser, char c)
    {
      serde::detail::write_u00_escape(ser, c);
    }
  };

  struct yaml_escapes
  {
    static constexpr bool is_control_byte(char c)
    {
      return serde::detail::is_control(c);
    }

    // The two-character escapes YAML names, or '\0' when the byte has none and
    // needs the \xXX form.
    static constexpr char two_char(char c)
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

    template <typename Ser> static void write_fallback(Ser& ser, char c)
    {
      serde::detail::write_hex2_escape(ser, c);
    }
  };

  // TOML 1.1: \0, \a and \v have no spelling in TOML at all.
  struct toml_escapes
  {
    // "Any Unicode character may be used except those that must be escaped:
    // quotation mark, backslash, and the control characters other than tab."
    // Tab is in here anyway: an escaped one is legal, and a literal tab inside a
    // value is a byte a reader cannot see.
    static constexpr bool is_control_byte(char c)
    {
      return serde::detail::is_control(c);
    }

    static constexpr char two_char(char c)
    {
      switch(c)
      {
        case '\b':
          return 'b';
        case '\t':
          return 't';
        case '\n':
          return 'n';
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

    // \xHH is a 1.1 addition a 1.0 reader rejects, and the only such form this
    // backend emits - reachable solely for a control byte with no compact escape.
    template <typename Ser> static void write_fallback(Ser& ser, char c)
    {
      serde::detail::write_hex2_escape(ser, c);
    }
  };

  // One entry per byte value, true when the byte cannot appear literally inside
  // a quoted string of this flavour. Bytes 0x80 and above are absent, they are
  // UTF-8 lead and continuation bytes and pass through unchanged.
  template <typename Flavour, char Quote>
  inline constexpr std::array<bool, 256> escape_table = [] {
    std::array<bool, 256> t{};
    for(unsigned c = 0; c < t.size(); ++c)
    {
      t[c] = Flavour::is_control_byte(static_cast<char>(c));
    }
    t[static_cast<unsigned char>(Quote)] = true;
    t[static_cast<unsigned char>('\\')]  = true;
    return t;
  }();

  template <typename Flavour, char Quote> constexpr bool needs_escape(char c)
  {
    return serde::detail::escape_table<Flavour, Quote>[static_cast<unsigned char>(c)];
  }

  // A bulk scan is wrong here. JSON's escape set is thirty-four bytes, so a min
  // over find(char) is thirty-four memchr passes and find_first_of is a per-byte
  // loop rescanning all thirty-four. The table walk beats both.
  template <typename Flavour, char Quote> std::size_t find_escapable(std::string_view s)
  {
    const auto* const first = s.data();
    const auto* const last  = first + s.size();
    const auto*       it    = std::find_if(first, last, serde::detail::needs_escape<Flavour, Quote>);
    return it == last ? std::string_view::npos : static_cast<std::size_t>(it - first);
  }

  template <typename Flavour, char Quote, typename Ser> void write_escape(Ser& ser, char c)
  {
    if(c == Quote or c == '\\')
    {
      ser.write_char('\\');
      ser.write_char(c);
      return;
    }
    if(const char esc = Flavour::two_char(c); esc != '\0')
    {
      ser.write_char('\\');
      ser.write_char(esc);
      return;
    }
    Flavour::write_fallback(ser, c);
  }

  // The body of a quoted string, without the quotes: runs of clean bytes go out
  // whole, only the bytes needing an escape are handled one at a time.
  template <typename Flavour, char Quote, typename Ser>
  void write_escaped(Ser& ser, std::string_view text)
  {
    serde::detail::write_with_escapes(
        ser,
        text,
        [](std::string_view s, std::size_t pos) {
          const std::size_t n = serde::detail::find_escapable<Flavour, Quote>(s.substr(pos));
          return n == std::string_view::npos ? n : pos + n;
        },
        [](Ser& out, char c) { serde::detail::write_escape<Flavour, Quote>(out, c); });
  }

  template <typename Flavour, char Quote, typename Ser>
  void write_backslash_quoted(Ser& ser, std::string_view text)
  {
    ser.write_char(Quote);
    serde::detail::write_escaped<Flavour, Quote>(ser, text);
    ser.write_char(Quote);
  }

  // A hexadecimal digit's value, or -1.
  constexpr int hex_value(char c)
  {
    if(c >= '0' and c <= '9')
    {
      return c - '0';
    }
    if(c >= 'a' and c <= 'f')
    {
      return c - 'a' + 10;
    }
    if(c >= 'A' and c <= 'F')
    {
      return c - 'A' + 10;
    }
    return -1;
  }

  // \xHH, \uHHHH and \UHHHHHHHH differ only in their digit count. `next` yields
  // the next body byte and throws when there is none, `invalid` returns the
  // message for a non-digit. The caller keeps the range check: what an
  // out-of-range code point is called is the format's business.
  template <typename Next, typename Invalid>
  int decode_hex_escape(int count, Next&& next, Invalid&& invalid)
  {
    int value = 0;
    for(int i = 0; i < count; ++i)
    {
      const char d = next();
      const int  n = serde::detail::hex_value(d);
      if(n < 0)
      {
        throw std::runtime_error(invalid(d));
      }
      value = (value << 4) | n;
    }
    return value;
  }
} // namespace reflex::serde::detail
