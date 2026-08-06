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

  // A value that occupies lines of its own, rather than sitting after "key: ".
  // The compile-time half of the separator decision.
  //
  // std::array<char, N> is an aggregate and a str_c, and it is a scalar here, so
  // the str_c guard is load-bearing rather than defensive.
  //
  // pair_c is in here because a standalone pair is written as a one-entry block
  // mapping. Leaving it out puts "k: v" after the parent's own "key: " and
  // produces "outer: k: v", which is not a document.
  template <typename T>
  concept block_node_c = (aggregate_c<std::remove_cvref_t<T>> or seq_c<std::remove_cvref_t<T>>
                          or map_c<std::remove_cvref_t<T>> or pair_c<std::remove_cvref_t<T>>)
                     and not str_c<std::remove_cvref_t<T>>;

  template <typename T> consteval std::size_t member_count()
  {
    return nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
  }

  // The runtime half. A collection with no entries has no lines to indent, so it
  // goes inline as [] or {} - which is also the only way YAML can spell one.
  template <typename T> bool is_inline_empty(T const& value)
  {
    if constexpr(seq_c<T> or map_c<T>)
    {
      return value.empty();
    }
    else if constexpr(aggregate_c<T>)
    {
      return member_count<T>() == 0;
    }
    else
    {
      return false;
    }
  }

  // "name:", built once at compile time and promoted to static storage.
  //
  // The key is checked here rather than escaped at runtime. An identifier can
  // never need quoting, but a serde::rename can carry an arbitrary string, and
  // annotations.hpp only bars a dot, a quote, a backslash and control characters
  // - not a colon, a leading dash, or a name that resolves as a boolean. So the
  // same predicate the scalar writer uses runs here too, which is why it and
  // everything under it are constexpr.
  template <std::meta::info Member> consteval std::string_view plain_key()
  {
    constexpr std::string_view name = serialized_name(Member);
    std::string                s;
    s.reserve(name.size() + 3);
    if(needs_quoting(name))
    {
      s += '\'';
      for(char c : name)
      {
        s += c;
        if(c == '\'')
        {
          s += c;
        }
      }
      s += '\'';
    }
    else
    {
      s += name;
    }
    s += ':';
    return {std::define_static_string(s), s.size()};
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
    int depth_ = 0;

  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "YAML";

    // Fixed, not an option. A configurable width would have to be threaded
    // through every tag_invoke, and no serde backend carries an options object.
    // If it is ever wanted it becomes a template parameter on this class.
    static constexpr int indent_width = 2;

    // THE INVARIANT, which the whole block writer rests on:
    //
    //   A block-node writer never emits the indent for its own first line, and
    //   never emits a trailing newline. Before every line after the first it
    //   emits '\n' followed by indent(depth).
    //
    // The parent is therefore what positions the cursor, which it already does
    // by having just written "key: " or "- ". Three things fall out: the
    // document has no leading or trailing newline, so a round trip is
    // byte-exact; the compact notations "- - 1" and "- key: v" need no special
    // case; and no writer ever needs to know what column it is at, only what
    // depth it was handed.
    //
    // The opposite arrangement - a block node emitting its own indent - forces
    // every parent to strip that first indent back off, and then every nesting
    // form needs its own case.
    void newline()
    {
      this->write_char('\n');
      for(int i = 0; i < depth_ * indent_width; ++i)
      {
        this->write_char(' ');
      }
    }

    // RAII so a throwing member cannot leave the depth wrong. [[nodiscard]] on
    // push() so `ser.push();` as a statement, which would pop immediately, does
    // not compile.
    class indent_guard
    {
      serializer* s_;

    public:
      explicit indent_guard(serializer& s) : s_{&s}
      {
        ++s_->depth_;
      }
      indent_guard(indent_guard const&)            = delete;
      indent_guard& operator=(indent_guard const&) = delete;
      ~indent_guard()
      {
        --s_->depth_;
      }
    };

    [[nodiscard]] indent_guard push()
    {
      return indent_guard{*this};
    }

    // The separator decision, for a value that follows "key:".
    //
    // A scalar goes on the same line after a space. A non-empty block node goes
    // on the next line, one level deeper. An empty one goes back on the same
    // line, because [] and {} are the only spelling YAML has for it.
    //
    // reflex::visit on a non-variant is the identity call, so this one spelling
    // covers a plain member and a poly::var member alike.
    template <typename T> void write_member(T const& value)
    {
      reflex::visit([this](auto const& v) { this->write_value<true>(v); }, value);
    }

    // The same decision for a sequence entry, which has just written "- ". No
    // leading space (the "- " supplied it) and no line break before a block
    // child: the child starts right there, which is the compact notation. That
    // one difference is the whole of it.
    template <typename T> void write_element(T const& value)
    {
      reflex::visit([this](auto const& v) { this->write_value<false>(v); }, value);
    }

  private:
    template <bool BreakBefore, typename T> void write_value(T const& value)
    {
      // An engaged optional of an aggregate is a block node, but std::optional
      // is not itself one, so the test has to see through it. Unwrapping here
      // rather than widening block_node_c keeps a disengaged optional inline as
      // "null", which is what it must be.
      if constexpr(meta::is_template_instance_of(^^T, ^^std::optional))
      {
        if(not value.has_value())
        {
          if constexpr(BreakBefore)
          {
            this->write_char(' ');
          }
          this->write_raw("null");
          return;
        }
        this->write_value<BreakBefore>(*value);
        return;
      }
      else if constexpr(detail::block_node_c<T>)
      {
        if(not detail::is_inline_empty(value))
        {
          auto guard = this->push();
          if constexpr(BreakBefore)
          {
            this->newline();
          }
          serialize(*this, value);
          return;
        }
        if constexpr(BreakBefore)
        {
          this->write_char(' ');
        }
        serialize(*this, value);
        return;
      }
      else
      {
        if constexpr(BreakBefore)
        {
          this->write_char(' ');
        }
        serialize(*this, value);
        return;
      }
    }

  public:
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

    // One "- " per element at this node's depth. The element goes through the
    // same decision as a mapping value, minus the leading space the "- "
    // already supplied and minus the line break: a block child starts right
    // after the dash, which is YAML's compact notation.
    //
    // The indent works out with no special case. "- " occupies two columns at
    // depth*2, so the child's continuation lines belong at depth*2 + 2, which is
    // exactly (depth+1)*2 - what the guard already produces.
    template <seq_c Seq>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Seq const& value)
    {
      if(value.empty())
      {
        ser.write_raw("[]");
        return ser.out();
      }

      bool first = true;
      for(auto const& elem : value)
      {
        if(not first)
        {
          ser.newline();
        }
        first = false;
        ser.write_raw("- ");
        ser.write_element(elem);
      }
      return ser.out();
    }

    // A block mapping with runtime keys. The key is serialized rather than
    // rendered and re-quoted: that way an int key stays plain so it resolves
    // back to an int, while a std::string key goes through the scalar writer
    // and picks up quotes only when a plain form would change its type.
    template <map_c Map>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Map const& value)
    {
      static_assert(
          not detail::block_node_c<typename Map::key_type>,
          std::string(display_string_of(dealias(^^typename Map::key_type)))
              + " cannot be a YAML mapping key: a composite key needs the explicit-key syntax"
                " ('? '), which this backend does not support");

      if(value.empty())
      {
        ser.write_raw("{}");
        return ser.out();
      }

      bool first = true;
      for(auto const& [key, val] : value)
      {
        if(not first)
        {
          ser.newline();
        }
        first = false;
        serialize(ser, key);
        ser.write_char(':');
        ser.write_member(val);
      }
      return ser.out();
    }

    // A standalone pair is a one-entry mapping. block_node_c names pair_c for
    // this reason.
    template <pair_c Pair>
    friend OutputIt
        tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Pair const& value)
    {
      serialize(ser, value.first);
      ser.write_char(':');
      ser.write_member(value.second);
      return ser.out();
    }

    // A poly::var reached directly, rather than as a member. write_member and
    // write_element already visit, so this is the top-level entry point.
    template <visitable_c T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
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

  // A block mapping, one "key: value" per line at this node's depth. No braces,
  // no separators: the extent of the mapping is its indentation.
  template <typename OutputIt, aggregate_c Agg>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char, N> is an aggregate
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value)
  {
    static constexpr auto type = decay(type_of(^^value));

    if constexpr(detail::member_count<Agg>() == 0)
    {
      ser.write_raw("{}");
      return ser.out();
    }
    else
    {
      bool first = true;
      template for(constexpr auto member : define_static_array(
                       nonstatic_data_members_of(type, std::meta::access_context::current())))
      {
        if(not first)
        {
          ser.newline();
        }
        first = false;

        static constexpr std::string_view key_token = detail::plain_key<member>();
        ser.write_raw(key_token);
        ser.write_member(value.[:member:]);
      }
      return ser.out();
    }
  }

  template <std::input_iterator InputIt>
  class deserializer : public serde::detail::subrange_deserializer<InputIt>
  {
    using base = serde::detail::subrange_deserializer<InputIt>;
    using base::cursor_;

    // Bounded pushback. YAML cannot be lexed with one byte of lookahead: "- "
    // is a sequence entry and "-1" is a number, ": " ends a plain scalar and
    // ":" does not, and "---" is a marker. A stream cursor is an input
    // iterator, so it cannot be copied and re-read to answer those - hence a
    // small buffer the parser peeks through instead.
    //
    // Bytes move out of cursor_ into here, so at_end() has to account for both.
    // It is shadowed below rather than inherited for exactly that reason.
    static constexpr std::size_t max_lookahead = 8;

    std::array<char, max_lookahead> ahead_{};
    std::size_t                     ahead_n_ = 0;

    // 0-based column of the next byte to be read. Every block-structure
    // decision is a comparison between two of these, so it has exactly one
    // maintainer: advance() increments it and next_line() zeroes it. Nothing
    // else may move the cursor.
    std::size_t column_ = 0;

    // A "---" has been seen, or content has started. Either way a second one is
    // a second document.
    bool doc_started_ = false;

    // Raw consume of one byte, no column bookkeeping. peek() has already put
    // the byte in the buffer, so this only ever pops.
    char take_()
    {
      const char c = peek();
      std::shift_left(ahead_.begin(), ahead_.begin() + ahead_n_, 1);
      --ahead_n_;
      return c;
    }

  public:
    using base::base;

    static constexpr std::size_t npos = std::size_t(-1);

    // makes load() without an explicit type read a yaml::value
    using default_load_type = yaml::value;

    bool at_end() const
    {
      return ahead_n_ == 0 and cursor_.empty();
    }

    // The byte `i` ahead of the cursor, or '\0' at end of input. Callers must
    // treat '\0' as "nothing there": a NUL in the input is not distinguishable
    // here, which is fine because a NUL is not valid YAML.
    char peek_at(std::size_t i)
    {
      while(ahead_n_ <= i)
      {
        if(cursor_.empty())
        {
          return '\0';
        }
        ahead_[ahead_n_++] = *cursor_.begin();
        cursor_.advance(1);
      }
      return ahead_[i];
    }

    char peek()
    {
      if(at_end())
      {
        throw std::runtime_error("Unexpected end of YAML input");
      }
      return peek_at(0);
    }

    // YAML 1.2 makes a lone '\r' ordinary content and only "\r\n" a break. That
    // needs two bytes of lookahead at every scalar byte, so this takes YAML
    // 1.1's rule instead: '\r' alone is a break too. The two differ only for a
    // lone CR inside a scalar, which no editor in use produces.
    static constexpr bool is_break(char c)
    {
      return c == '\n' or c == '\r';
    }

    bool at_line_end()
    {
      return at_end() or is_break(peek_at(0));
    }

    std::size_t column() const
    {
      return column_;
    }

    // Consumes one byte of the current line.
    //
    // Refuses a line break. Every caller has already tested at_line_end(), so a
    // break reaching here is a bug in the caller, not bad input - and a node
    // reader that runs past a line end has lost the column, which makes every
    // column after it wrong. Better to fail here than to mis-parse three lines
    // later.
    char advance()
    {
      const char c = peek();
      if(is_break(c))
      {
        throw std::runtime_error("YAML: internal - advance() over a line break");
      }
      take_();
      ++column_;
      return c;
    }

    void skip_to_line_end()
    {
      while(not at_line_end())
      {
        advance();
      }
    }

    // The only function that may consume a line break. Consumes the rest of the
    // current line too. Returns false at end of input.
    bool next_line()
    {
      while(not at_end() and not is_break(peek_at(0)))
      {
        take_();
      }
      if(at_end())
      {
        return false;
      }
      const char c = take_();
      if(c == '\r' and not at_end() and peek_at(0) == '\n')
      {
        take_();
      }
      column_ = 0;
      return true;
    }

    // The leading whitespace run of a line, returning the resulting column.
    // Must be called with the cursor at the start of a line.
    //
    // A tab may not indent a YAML node. It may appear on a line that carries no
    // node at all, so the error is raised only when content follows it.
    std::size_t skip_indent()
    {
      bool tab = false;
      while(not at_end())
      {
        const char c = peek_at(0);
        if(c == ' ')
        {
          advance();
          continue;
        }
        if(c == '\t')
        {
          tab = true;
          advance();
          continue;
        }
        break;
      }
      if(tab and not at_line_end() and peek_at(0) != '#')
      {
        throw std::runtime_error("YAML: a tab cannot be used to indent a node");
      }
      return column_;
    }

    // Everything a line may carry after a node has been read: trailing spaces
    // and a comment. Anything else is an error, caught here rather than left to
    // desynchronise the next line.
    void finish_line()
    {
      while(not at_line_end() and (peek_at(0) == ' ' or peek_at(0) == '\t'))
      {
        advance();
      }
      if(at_line_end())
      {
        return;
      }
      if(peek_at(0) == '#')
      {
        skip_to_line_end();
        return;
      }
      throw std::runtime_error("YAML: unexpected content after a value");
    }

    // Advance to the first line carrying a node, leaving the cursor on its
    // first non-space byte, and return that line's indent. npos at end of
    // input, or at a "..." document terminator.
    //
    // IDEMPOTENT when the cursor already sits on the first byte of a node.
    // Steps 07 and 08 depend on that: a nested block stops on the first line it
    // does not own, and its parent then asks this same question about that
    // line. Without it, a key is skipped every time a nested block ends.
    std::size_t next_content_line()
    {
      if(column_ == 0)
      {
        // A fresh line, nothing consumed from it yet.
        skip_indent();
        if(at_end())
        {
          return npos;
        }
        if(not at_line_end() and peek_at(0) != '#')
        {
          const std::size_t indent = on_content_();
          if(indent != marker_consumed_)
          {
            return indent;
          }
          // A "---" on the very first line: it has been consumed, and the node
          // is on a later line. Fall through to the loop.
        }
      }
      else if(not at_line_end() and peek_at(0) != ' ' and peek_at(0) != '\t'
              and peek_at(0) != '#')
      {
        return column_; // already parked on a node
      }
      else
      {
        finish_line();
      }

      while(true)
      {
        if(not next_line())
        {
          return npos;
        }
        skip_indent();
        if(at_end())
        {
          return npos;
        }
        if(at_line_end() or peek_at(0) == '#')
        {
          continue; // blank or comment-only
        }
        const std::size_t indent = on_content_();
        if(indent != marker_consumed_)
        {
          return indent;
        }
      }
    }

  private:
    static constexpr std::size_t marker_consumed_ = npos - 1;

    // Called with the cursor on the first byte of a node. Deals with the
    // document markers and directives, which are the features this backend
    // names rather than mis-parses.
    std::size_t on_content_()
    {
      if(column_ == 0 and marker_here_())
      {
        const char c = peek_at(0);
        if(c == '%')
        {
          throw std::runtime_error("YAML: directives (%YAML, %TAG) are not supported");
        }
        if(c == '.')
        {
          return npos; // "..." terminates the document
        }
        if(doc_started_)
        {
          throw std::runtime_error("YAML: multiple documents in one stream are not supported");
        }
        doc_started_ = true;
        advance();
        advance();
        advance();
        finish_line(); // a node on the "---" line itself is not supported
        return marker_consumed_;
      }
      doc_started_ = true;
      return column_;
    }

    // "---", "..." or a directive, at column 0. Needs four bytes of lookahead,
    // which is what the pushback buffer exists for.
    bool marker_here_()
    {
      const char c = peek_at(0);
      if(c == '%')
      {
        return true;
      }
      if(c != '-' and c != '.')
      {
        return false;
      }
      if(peek_at(1) != c or peek_at(2) != c)
      {
        return false;
      }
      const char after = peek_at(3);
      return after == '\0' or after == ' ' or after == '\t' or is_break(after);
    }
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
