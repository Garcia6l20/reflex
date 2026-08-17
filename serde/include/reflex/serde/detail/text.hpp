#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <algorithm>
#include <array>
#include <charconv>
#include <concepts>
#include <cstring>
#include <format>
#include <initializer_list>
#include <limits>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <type_traits>

#include <reflex/constant.hpp>
#include <reflex/meta.hpp>
#endif

REFLEX_EXPORT namespace reflex::serde::detail
{
  constexpr bool is_dec_digit(char c)
  {
    return c >= '0' and c <= '9';
  }

  constexpr bool is_bin_digit(char c)
  {
    return c == '0' or c == '1';
  }

  constexpr bool is_oct_digit(char c)
  {
    return c >= '0' and c <= '7';
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

  constexpr bool equals(std::string_view a, std::string_view b, bool fold_case)
  {
    return fold_case ? iequals(a, b) : a == b;
  }

  // What a format's numeric grammar accepts: one matcher and one parser, two
  // constants.
  //
  // The spellings are constant_string and not std::string_view because an NTTP
  // type has to be structural, which requires every non-static data member to be
  // public, and std::string_view keeps its pointer and length private. That is
  // the standard's rule rather than a GCC limitation.
  struct number_syntax
  {
    bool underscores = false; // TOML: 1_000_000. YAML: no.
    bool binary      = false; // TOML: 0b1010. YAML: no.
    bool octal       = false; // both, spelled 0o
    bool hexadecimal = false; // both, spelled 0x
    // How the format spells the non-finite floats. YAML wants ".inf"/".nan"
    // case-insensitively, TOML wants "inf"/"nan" lowercase only.
    constant_string inf_text  = "inf";
    constant_string nan_text  = "nan";
    bool            fold_case = false;
    // Whether a sign may precede the not-a-number spelling. YAML's core schema
    // signs the infinities and not the NaN, so "-.nan" is a string there; TOML
    // signs both. The infinities take a sign in both, so they need no knob.
    bool signed_nan = false;
  };

  static_assert(std::meta::is_structural_type(^^number_syntax));

  inline constexpr number_syntax yaml_numbers{
      .octal       = true,
      .hexadecimal = true,
      .inf_text    = ".inf",
      .nan_text    = ".nan",
      .fold_case   = true};

  inline constexpr number_syntax toml_numbers{
      .underscores = true,
      .binary      = true,
      .octal       = true,
      .hexadecimal = true,
      .signed_nan  = true};

  // A run of digits, with '_' accepted only between two of them and only when the
  // format allows separators. An empty run is never one.
  template <typename Pred>
  constexpr bool is_digit_run(std::string_view s, Pred digit, bool separators)
  {
    if(s.empty() or not digit(s.front()) or not digit(s.back()))
    {
      return false;
    }
    bool prev_separator = false;
    for(const char c : s)
    {
      if(digit(c))
      {
        prev_separator = false;
        continue;
      }
      if(not separators or c != '_' or prev_separator)
      {
        return false;
      }
      prev_separator = true;
    }
    return true;
  }

  // Advances `i` past a run of `digit` bytes, accepting a separator only between
  // two of them, and sets `any` when the run held at least one digit. A
  // separator at an edge of the run returns false and ends the whole match.
  template <typename Pred>
  constexpr bool
      scan_digits(std::string_view s, std::size_t& i, Pred digit, bool separators, bool& any)
  {
    while(i < s.size())
    {
      if(digit(s[i]))
      {
        any = true;
        ++i;
        continue;
      }
      if(not separators or s[i] != '_')
      {
        break;
      }
      if(i == 0 or not digit(s[i - 1]) or i + 1 >= s.size() or not digit(s[i + 1]))
      {
        return false;
      }
      ++i;
    }
    return true;
  }

  // The integer forms: [-+]?[0-9]+, plus whichever of 0b, 0o and 0x the syntax
  // turns on. A sign is decimal-only in both formats, which is why the prefix
  // tests come first.
  template <number_syntax Syntax> constexpr bool matches_int(std::string_view s)
  {
    if constexpr(Syntax.binary)
    {
      if(s.starts_with("0b"))
      {
        return is_digit_run(s.substr(2), is_bin_digit, Syntax.underscores);
      }
    }
    if constexpr(Syntax.octal)
    {
      if(s.starts_with("0o"))
      {
        return is_digit_run(s.substr(2), is_oct_digit, Syntax.underscores);
      }
    }
    if constexpr(Syntax.hexadecimal)
    {
      if(s.starts_with("0x"))
      {
        return is_digit_run(s.substr(2), is_hex_digit, Syntax.underscores);
      }
    }
    if(s.starts_with('-') or s.starts_with('+'))
    {
      s.remove_prefix(1);
    }
    return is_digit_run(s, is_dec_digit, Syntax.underscores);
  }

  // [-+]? ( \.[0-9]+ | [0-9]+ (\.[0-9]*)? ) ( [eE][-+]?[0-9]+ )?, plus the
  // non-finite spellings the syntax names.
  //
  // Hand-rolled rather than probed with std::from_chars because this has to run
  // at compile time for yaml's plain_key(), and the floating-point overload of
  // from_chars is not constexpr. Doing it by hand also keeps the accepted set
  // exactly the format's rather than whatever from_chars happens to take - it
  // would accept "0x1p3" and "infinity", which neither format resolves as a
  // number.
  template <number_syntax Syntax> constexpr bool matches_float(std::string_view s)
  {
    constexpr std::string_view inf_text{*Syntax.inf_text};
    constexpr std::string_view nan_text{*Syntax.nan_text};

    const bool signed_ = s.starts_with('-') or s.starts_with('+');
    if(signed_)
    {
      s.remove_prefix(1);
    }
    if(equals(s, inf_text, Syntax.fold_case))
    {
      return true;
    }
    if((Syntax.signed_nan or not signed_) and equals(s, nan_text, Syntax.fold_case))
    {
      return true;
    }

    std::size_t i      = 0;
    bool        digits = false;
    if(not scan_digits(s, i, is_dec_digit, Syntax.underscores, digits))
    {
      return false;
    }
    if(i < s.size() and s[i] == '.')
    {
      ++i;
      if(not scan_digits(s, i, is_dec_digit, Syntax.underscores, digits))
      {
        return false;
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
      // `digits` is true by here and is never read again, so it doubles as the
      // exponent scan's sink.
      if(not scan_digits(s, i, is_dec_digit, Syntax.underscores, digits))
      {
        return false;
      }
      if(i == exp_start)
      {
        return false;
      }
    }
    return i == s.size();
  }

  // from_chars takes no separators, so a number carrying them is copied into a
  // stack buffer with the underscores removed. Bounded: an integer past 64 bits
  // is out of range anyway, and a float past this is not representable, so a
  // longer literal is rejected here rather than silently truncated.
  inline constexpr std::size_t max_number_digits = 96;

  using number_buffer = std::array<char, max_number_digits>;

  // A view of `text` without its separators, using `buf` as storage. The result
  // aliases `text` when there is nothing to strip, so the common case copies
  // nothing.
  inline std::string_view
      strip_separators(std::string_view text, number_buffer& buf, std::string_view format_name)
  {
    if(text.find('_') == std::string_view::npos)
    {
      return text;
    }
    if(text.size() > buf.size())
    {
      throw std::runtime_error(
          std::format("{}: '{}' is too long to be a number", format_name, text));
    }
    const auto out = std::ranges::remove_copy(text, buf.begin(), '_').out;
    return std::string_view{buf.data(), static_cast<std::size_t>(out - buf.begin())};
  }

  template <number_syntax Syntax, typename Num>
  Num parse_number(std::string_view text, std::string_view format_name)
  {
    [[maybe_unused]] number_buffer buf{};
    std::string_view               body_text = text;
    if constexpr(Syntax.underscores)
    {
      body_text = strip_separators(text, buf, format_name);
    }

    if constexpr(std::floating_point<Num>)
    {
      constexpr std::string_view inf_text{*Syntax.inf_text};
      constexpr std::string_view nan_text{*Syntax.nan_text};

      std::string_view spelling = body_text;
      const bool       neg      = spelling.starts_with('-');
      if(neg or spelling.starts_with('+'))
      {
        spelling.remove_prefix(1);
      }
      const bool signed_ = spelling.size() != body_text.size();
      if((Syntax.signed_nan or not signed_) and equals(spelling, nan_text, Syntax.fold_case))
      {
        return std::numeric_limits<Num>::quiet_NaN();
      }
      if(equals(spelling, inf_text, Syntax.fold_case))
      {
        return neg ? -std::numeric_limits<Num>::infinity()
                   : std::numeric_limits<Num>::infinity();
      }
      // The whole text goes to from_chars, not the sign-stripped body: it takes
      // a '-' and rejects a '+', and that is the accepted set.
      Num        value{};
      const auto last = body_text.data() + body_text.size();
      auto [ptr, ec]  = std::from_chars(body_text.data(), last, value);
      if(ec != std::errc{} or ptr != last)
      {
        throw std::runtime_error(std::format("{}: '{}' is not a number", format_name, text));
      }
      return value;
    }
    else
    {
      // from_chars takes neither a '+' nor a "0x"/"0o"/"0b" prefix, so the sign
      // and the base come off here and the digits go in bare.
      std::string_view body = body_text;
      bool             neg  = false;
      if(body.starts_with('-'))
      {
        neg = true;
        body.remove_prefix(1);
      }
      else if(body.starts_with('+'))
      {
        body.remove_prefix(1);
      }
      int base = 10;
      if constexpr(Syntax.hexadecimal)
      {
        if(body.starts_with("0x"))
        {
          base = 16;
          body.remove_prefix(2);
        }
      }
      if constexpr(Syntax.octal)
      {
        if(base == 10 and body.starts_with("0o"))
        {
          base = 8;
          body.remove_prefix(2);
        }
      }
      if constexpr(Syntax.binary)
      {
        if(base == 10 and body.starts_with("0b"))
        {
          base = 2;
          body.remove_prefix(2);
        }
      }
      Num        value{};
      const auto last = body.data() + body.size();
      auto [ptr, ec]  = std::from_chars(body.data(), last, value, base);
      if(ec != std::errc{} or ptr != last)
      {
        throw std::runtime_error(std::format("{}: '{}' is not an integer", format_name, text));
      }
      if(neg)
      {
        if constexpr(std::is_signed_v<Num>)
        {
          value = static_cast<Num>(-value);
        }
        else
        {
          throw std::runtime_error(std::format(
              "{}: '{}' is negative and the destination is unsigned", format_name, text));
        }
      }
      return value;
    }
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
} // namespace reflex::serde::detail
