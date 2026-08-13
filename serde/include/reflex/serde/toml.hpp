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

  // A header path is assembled before any of it is written.
  inline std::string encoded_key(std::string_view name)
  {
    std::string out;
    if(is_bare_key(name))
    {
      out = name;
      return out;
    }
    serde::detail::serializer_base<std::back_insert_iterator<std::string>> sink{out};
    serde::detail::write_backslash_quoted<escapes, '"'>(sink, name);
    return out;
  }

  template <typename K> std::string_view key_view(K const& key, std::string& scratch)
  {
    if constexpr(str_c<std::remove_cvref_t<K>>)
    {
      return string_view_of(key);
    }
    else if constexpr(std::same_as<std::remove_cvref_t<K>, char>)
    {
      scratch.assign(1, key);
      return scratch;
    }
    else
    {
      scratch = std::format("{}", key);
      return scratch;
    }
  }

  // What a table pass classifies a member by: an engaged optional of a table
  // still gets a [header], an empty one is dropped.
  template <typename T>
  using shape_t = typename serde::detail::field_value<std::remove_cvref_t<T>>::type;

  // std::array<char, N> is an aggregate and must stay a scalar, so the str_c
  // guard is load-bearing. pair_c is here because a standalone pair is a one-entry
  // table; without it "k = v" follows the parent's own "key = " and produces
  // "outer = k = v", which is not a document.
  template <typename T>
  concept header_table_c = (aggregate_c<std::remove_cvref_t<T>> or map_c<std::remove_cvref_t<T>>
                            or pair_c<std::remove_cvref_t<T>>)
                       and not str_c<std::remove_cvref_t<T>>
                       and not derives_c<std::remove_cvref_t<T>, derive_t<Format>>;

  template <typename T>
  concept table_array_c = seq_c<std::remove_cvref_t<T>>
                      and header_table_c<typename std::remove_cvref_t<T>::value_type>;

  template <typename T> consteval std::size_t member_count()
  {
    return nonstatic_data_members_of(^^T, std::meta::access_context::current()).size();
  }
  } // namespace detail

  template <typename OutputIt> class serializer : public serde::detail::serializer_base<OutputIt>
  {
    // The dotted path of the table being written, without brackets: "server.tls".
    // Grown through a scope guard so a throwing member cannot leave it stale.
    std::string path_;

    // Non-zero inside an array or an inline table, where a [header] has no line.
    int inline_depth_ = 0;

    // The break goes before a line rather than after, so a document has neither a
    // leading nor a trailing newline.
    bool wrote_line_ = false;

  public:
    using serde::detail::serializer_base<OutputIt>::serializer_base;

    static constexpr std::string_view format_name = "TOML";

    class path_guard
    {
      serializer* s_;
      std::size_t size_;

    public:
      path_guard(serializer& s, std::string_view key) : s_{&s}, size_{s.path_.size()}
      {
        if(not s.path_.empty())
        {
          s.path_ += '.';
        }
        s.path_ += key;
      }
      path_guard(path_guard const&)            = delete;
      path_guard& operator=(path_guard const&) = delete;
      ~path_guard()
      {
        s_->path_.resize(size_);
      }
    };

    class inline_guard
    {
      serializer* s_;

    public:
      explicit inline_guard(serializer& s) : s_{&s}
      {
        ++s_->inline_depth_;
      }
      inline_guard(inline_guard const&)            = delete;
      inline_guard& operator=(inline_guard const&) = delete;
      ~inline_guard()
      {
        --s_->inline_depth_;
      }
    };

    bool inline_mode() const
    {
      return inline_depth_ > 0;
    }

    [[nodiscard]] inline_guard push_inline()
    {
      return inline_guard{*this};
    }

    std::string_view path() const
    {
      return path_;
    }

    void begin_line()
    {
      if(wrote_line_)
      {
        this->write_char('\n');
      }
      wrote_line_ = true;
    }

    // A poly::var only finds out what it is when it is visited.
    template <typename T> void write_inline(T const& value)
    {
      const inline_guard guard{*this};
      reflex::visit([this](auto const& v) { serialize(*this, v); }, value);
    }

    // TOML has no null, so an empty optional takes its key with it. Only this can
    // drop a key, which is why the value-side overload throws instead.
    template <typename T> void write_pair(std::string_view key_token, T const& value)
    {
      if constexpr(optional_c<std::remove_cvref_t<T>>)
      {
        if(value.has_value())
        {
          write_pair(key_token, *value);
        }
      }
      else
      {
        begin_line();
        this->write_raw(key_token);
        write_inline(value);
      }
    }

    template <typename T> void write_runtime_pair(std::string_view key, T const& value)
    {
      if constexpr(optional_c<std::remove_cvref_t<T>>)
      {
        if(value.has_value())
        {
          write_runtime_pair(key, *value);
        }
      }
      else
      {
        begin_line();
        detail::write_key(*this, key);
        this->write_raw(" = ");
        write_inline(value);
      }
    }

    // Returns the next value of `first`, which an omitted key leaves untouched.
    template <typename T>
    bool write_inline_pair(bool first, std::string_view key_token, T const& value)
    {
      if constexpr(optional_c<std::remove_cvref_t<T>>)
      {
        if(not value.has_value())
        {
          return first;
        }
        return write_inline_pair(first, key_token, *value);
      }
      else
      {
        if(not first)
        {
          this->write_raw(", ");
        }
        this->write_raw(key_token);
        write_inline(value);
        return false;
      }
    }

    // `key` arrives already bare or quoted. An empty table still gets its header:
    // [empty] with nothing under it is a distinct TOML value.
    template <typename T> void write_table(std::string_view key, T const& value)
    {
      if constexpr(optional_c<std::remove_cvref_t<T>>)
      {
        if(value.has_value())
        {
          write_table(key, *value);
        }
      }
      else
      {
        const path_guard guard{*this, key};
        begin_line();
        this->write_char('[');
        this->write_raw(path_);
        this->write_char(']');
        serialize(*this, value);
      }
    }

    // One "[[path.key]]" per element. An empty sequence writes nothing: [[x]] with
    // no body is one element, not zero.
    template <typename Seq> void write_table_array(std::string_view key, Seq const& value)
    {
      const path_guard guard{*this, key};
      for(auto const& elem : value)
      {
        begin_line();
        this->write_raw("[[");
        this->write_raw(path_);
        this->write_raw("]]");
        serialize(*this, elem);
      }
    }

    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, null_t const&)
    {
      throw std::runtime_error(
          "TOML has no null: a null value can only be omitted along with its key, and an array "
          "element has no key to omit");
      return ser.out();
    }

    template <typename T>
    friend OutputIt tag_invoke(
        tag_t<serde::serialize>,
        serializer<OutputIt>&   ser,
        std::optional<T> const& value)
    {
      if(not value.has_value())
      {
        throw std::runtime_error(
            "TOML has no null: an empty optional can only be omitted along with its key, and an "
            "array element has no key to omit");
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

    // A sequence of tables never gets here: the table writer's third pass emits
    // [[headers]] for it directly.
    template <seq_c Seq>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Seq const& value)
    {
      const inline_guard guard{ser};
      ser.write_char('[');
      bool first = true;
      for(auto const& elem : value)
      {
        if(not first)
        {
          ser.write_raw(", ");
        }
        first = false;
        reflex::visit([&ser](auto const& v) { serialize(ser, v); }, elem);
      }
      ser.write_char(']');
      return ser.out();
    }

    // Every entry of a map has the same shape, so the three passes collapse to one
    // loop and a compile-time question about mapped_type.
    template <map_c Map>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Map const& value)
    {
      using shape = detail::shape_t<typename Map::mapped_type>;

      if(ser.inline_mode())
      {
        ser.write_raw(value.empty() ? "{" : "{ ");
        bool        first = true;
        std::string scratch;
        for(auto const& [key, val] : value)
        {
          if(not first)
          {
            ser.write_raw(", ");
          }
          detail::write_key(ser, detail::key_view(key, scratch));
          ser.write_raw(" = ");
          ser.write_inline(val);
          first = false;
        }
        ser.write_raw(value.empty() ? "}" : " }");
        return ser.out();
      }

      std::string scratch;
      for(auto const& [key, val] : value)
      {
        const std::string_view text = detail::key_view(key, scratch);
        if constexpr(detail::table_array_c<shape>)
        {
          ser.write_table_array(detail::encoded_key(text), val);
        }
        else if constexpr(detail::header_table_c<shape>)
        {
          ser.write_table(detail::encoded_key(text), val);
        }
        else
        {
          ser.write_runtime_pair(text, val);
        }
      }
      return ser.out();
    }

    template <pair_c Pair>
    friend OutputIt
        tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, Pair const& value)
    {
      std::string            scratch;
      const std::string_view key = detail::key_view(value.first, scratch);
      if(ser.inline_mode())
      {
        ser.write_raw("{ ");
        detail::write_key(ser, key);
        ser.write_raw(" = ");
        ser.write_inline(value.second);
        ser.write_raw(" }");
        return ser.out();
      }
      ser.write_runtime_pair(key, value.second);
      return ser.out();
    }

    template <visitable_c T>
    friend OutputIt tag_invoke(tag_t<serde::serialize>, serializer<OutputIt>& ser, T const& value)
    {
      return visit([&ser](const auto& v) { return serialize(ser, v); }, value);
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

  // Three passes over the members, and pass 1 must never emit a header: a key
  // written after a [subtable] header belongs to that subtable, so a `port` line
  // trailing a [server.tls] header becomes server.tls.port, not server.port - a
  // valid document that means something else. Passes 2 and 3 split only so plain
  // subtables precede arrays of tables; either order of those is correct.
  //
  // Inside an array or an inline table there is no line for a header to sit on,
  // so the same aggregate goes out inline. That is what inline_depth_ answers.
  template <typename OutputIt, aggregate_c Agg>
    requires(not(str_c<Agg> or seq_c<Agg>)) // std::array<char, N> is an aggregate
  OutputIt tag_invoke(
      tag_default_t<serde::serialize>, serializer<OutputIt> & ser, Agg const& value)
  {
    static constexpr auto type    = decay(type_of(^^value));
    static constexpr auto members = define_static_array(
        nonstatic_data_members_of(type, std::meta::access_context::current()));

    if constexpr(members.size() == 0)
    {
      if(ser.inline_mode())
      {
        ser.write_raw("{}");
      }
      return ser.out();
    }
    else
    {
      if(ser.inline_mode())
      {
        ser.write_raw("{ ");
        bool first = true;
        template for(constexpr auto member : members)
        {
          first = ser.write_inline_pair(first, detail::assign_key<member>(), value.[:member:]);
        }
        ser.write_raw(" }");
        return ser.out();
      }

      // Pass 1 - every leaf and every array of leaves, as "key = value".
      template for(constexpr auto member : members)
      {
        using shape = detail::shape_t<typename[:type_of(member):]>;
        if constexpr(not(detail::header_table_c<shape> or detail::table_array_c<shape>))
        {
          ser.write_pair(detail::assign_key<member>(), value.[:member:]);
        }
      }

      // Pass 2 - every table, as "[path.key]" and then its own body.
      template for(constexpr auto member : members)
      {
        using shape = detail::shape_t<typename[:type_of(member):]>;
        if constexpr(detail::header_table_c<shape>)
        {
          ser.write_table(detail::key_name<member>(), value.[:member:]);
        }
      }

      // Pass 3 - every array of tables, as repeated "[[path.key]]".
      template for(constexpr auto member : members)
      {
        using shape = detail::shape_t<typename[:type_of(member):]>;
        if constexpr(detail::table_array_c<shape>)
        {
          ser.write_table_array(detail::key_name<member>(), value.[:member:]);
        }
      }
      return ser.out();
    }
  }

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
