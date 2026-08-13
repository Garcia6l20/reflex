#pragma once

#ifndef REFLEX_EXPORT
#define REFLEX_EXPORT
#endif

#ifndef REFLEX_MODULE
#include <cmath>
#include <cstring>
#include <map>
#include <vector>

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
  using serde::detail::matches_float;
  using serde::detail::matches_int;
  using serde::detail::parse_number;
  using serde::detail::string_view_of;
  using serde::detail::toml_numbers;

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

  // How a path came to exist. A table is created once and extended never, but a
  // [header] may claim a path a longer [header] only implied, and a dotted key
  // may add a sibling to a table an earlier dotted key created.
  enum class path_kind
  {
    value,    // a "key = value" landed exactly here
    dotted,   // created by a dotted key: "a" in "a.b = 1"
    header,   // claimed by a [header], or an element of a [[header]]
    implied,  // created by a longer [header]: "a" in "[a.b]"
    array,    // claimed by a [[header]]; `count` is how many elements so far
  };

  struct path_entry
  {
    path_kind   kind;
    std::size_t count = 0;
  };

  inline constexpr std::size_t no_index = static_cast<std::size_t>(-1);

  // `index` is set only where the segment names an [[array of tables]], and says
  // which element the rest of the path is relative to.
  struct segment
  {
    std::string_view name;
    std::size_t      index = no_index;
  };

  // A path as a map key. Each segment carries its own length, so the two-segment
  // path a.b is not the one-segment key "a.b", and an array element carries its
  // index, so "[[a]] b = 1" twice names two paths and not one twice.
  inline std::string with_segment(std::string_view base, segment seg)
  {
    std::string out{base};
    out += std::to_string(seg.name.size());
    out += ':';
    out += seg.name;
    if(seg.index != no_index)
    {
      out += '@';
      out += std::to_string(seg.index);
    }
    return out;
  }

  // The same path for an error message, shown as it was decoded rather than
  // re-quoted.
  inline std::string join_path(std::span<segment const> path)
  {
    std::string out;
    for(segment seg : path)
    {
      if(not out.empty())
      {
        out += '.';
      }
      out += seg.name;
      if(seg.index != no_index)
      {
        out += std::format("[{}]", seg.index);
      }
    }
    return out;
  }

  // serde::object_visit cannot index into a sequence, which is what an [[array of
  // tables]] header addresses, so this applies the index after each hop. The
  // recursion walks the type graph and not the document, so it is finite; a walk
  // is bounded at run time by serde::max_key_depth. `full` is the whole path, so
  // a failure four segments in still says where it happened.
  template <typename Root, typename Fn>
  void table_visit(std::span<segment const> path, Root& root, Fn& fn, std::string_view full);

  // A growable sequence grows to reach the element, which is what "[[x]] appends
  // an element" means; a fixed-size one is bounds-checked instead.
  template <typename Seq>
  auto& element_at(Seq& seq, segment seg, std::string_view full)
  {
    if constexpr(requires { seq.emplace_back(); })
    {
      while(seq.size() <= seg.index)
      {
        seq.emplace_back();
      }
    }
    else
    {
      if(seg.index >= std::size(seq))
      {
        throw std::out_of_range(std::format(
            "TOML: in '{}', '{}' is element {} of an array of tables and the destination cannot "
            "grow to reach it",
            full, seg.name, seg.index));
      }
    }
    using diff = std::ranges::range_difference_t<Seq>;
    return *std::ranges::next(std::ranges::begin(seq), static_cast<diff>(seg.index));
  }

  template <typename Node, typename Fn>
  void table_descend(
      std::span<segment const> rest,
      Node&                    node,
      Fn&                      fn,
      std::string_view         full,
      std::string_view         name)
  {
    if(rest.empty())
    {
      fn(node);
      return;
    }
    if constexpr(object_visitable_c<std::remove_cvref_t<Node>>)
    {
      table_visit(rest, node, fn, full);
    }
    else
    {
      // object_visit leaves its callback uncalled on a miss, and this callback is
      // what reads the value, so the cursor would sit on unread text.
      throw std::runtime_error(std::format(
          "TOML: in '{}', '{}' is not a table and nothing can be written under it", full, name));
    }
  }

  template <typename Root, typename Fn>
  void table_visit(std::span<segment const> path, Root& root, Fn& fn, std::string_view full)
  {
    const segment seg  = path.front();
    const auto    rest = path.subspan(1);

    object_visitor<std::remove_cvref_t<Root>>{}(
        [&]<typename N>(N&& nested) {
          using U = std::remove_cvref_t<N>;
          // std::array<char, N> is both a sequence and a string and has to stay
          // a scalar, the same trap the table writers document.
          if constexpr(seq_c<U> and not str_c<U>)
          {
            if(seg.index != no_index)
            {
              table_descend(rest, element_at(nested, seg, full), fn, full, seg.name);
              return;
            }
          }
          else
          {
            if(seg.index != no_index)
            {
              throw std::runtime_error(std::format(
                  "TOML: in '{}', '{}' is an array of tables in the document and is not a "
                  "sequence in the destination",
                  full, seg.name));
            }
          }
          table_descend(rest, nested, fn, full, seg.name);
        },
        seg.name, root);
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
    using base = serde::detail::line_cursor<InputIt>;

    // One buffer, not a pool: a token is dead before the next one is read.
    std::string token_buf_;

    // Separate from token_buf_: a key and its value are read one after the other.
    std::string key_buf_;

    // Non-zero while a value is being read. The object_visitable_c tag_invoke is
    // both "read a document" and "read an inline table"; this tells them apart.
    int depth_ = 0;

    // The table every following assignment belongs to. The names point into
    // header_buf_ and only the next header rewrites it.
    std::vector<detail::segment> table_path_;
    std::string                  header_buf_;

    // Every path the document has claimed, and how; see detail::path_kind. Keyed
    // by the encoded path, so "[[a]] b = 1" twice is not a duplicate key.
    std::map<std::string, detail::path_entry> paths_;

  public:
    using base::base;
    using base::advance;
    using base::at_end;
    using base::at_line_end;
    using base::column;
    using base::is_break;
    using base::next_line;
    using base::peek;
    using base::peek_at;
    using base::skip_to_line_end;

    static constexpr std::string_view format_name = "TOML";

    // makes load() without an explicit type read a toml::value
    using default_load_type = toml::value;

    class value_scope
    {
      deserializer* d_;

    public:
      explicit value_scope(deserializer& d) : d_{&d}
      {
        ++d_->depth_;
      }
      value_scope(value_scope const&)            = delete;
      value_scope& operator=(value_scope const&) = delete;
      ~value_scope()
      {
        --d_->depth_;
      }
    };

    bool at_document_level() const
    {
      return depth_ == 0;
    }

    // TOML never makes indentation meaningful, so spaces and tabs are all of it.
    void skip_ws()
    {
      while(not at_line_end() and (peek_at(0) == ' ' or peek_at(0) == '\t'))
      {
        advance();
      }
    }

    // "non-eol = %x09 / %x20-7E / non-ascii": a stray control byte is reported
    // here rather than skipped over.
    void skip_comment()
    {
      advance(); // '#'
      while(not at_line_end())
      {
        const char c = peek_at(0);
        if(c != '\t' and serde::detail::is_control(c))
        {
          throw std::runtime_error(
              "TOML: a control character other than tab cannot appear in a comment");
        }
        advance();
      }
    }

    // Caught here rather than left to desynchronise the next line.
    void finish_line()
    {
      skip_ws();
      if(at_line_end())
      {
        return;
      }
      if(peek_at(0) == '#')
      {
        skip_comment();
        return;
      }
      throw std::runtime_error("TOML: unexpected content after a value");
    }

    // Idempotent when the cursor already sits on content: read_document()'s loop
    // asks this about a line a value reader has already stopped on.
    bool next_content_line()
    {
      while(true)
      {
        skip_ws();
        if(at_end())
        {
          return false;
        }
        if(not at_line_end())
        {
          if(peek_at(0) != '#')
          {
            return true;
          }
          skip_comment();
        }
        if(not next_line())
        {
          return false;
        }
      }
    }

    bool at_string()
    {
      if(at_line_end())
      {
        return false;
      }
      const char c = peek_at(0);
      return c == '"' or c == '\'';
    }

    // The four string forms differ in three bits - quote character, delimiter run,
    // whether escapes are honoured - so one reader covers all four.
    std::string read_string()
    {
      const char quote = peek();
      if(quote != '"' and quote != '\'')
      {
        throw std::runtime_error(std::format("TOML: expected a quoted string, got '{}'", quote));
      }
      int run = 1;
      if(peek_at(1) == quote and peek_at(2) == quote)
      {
        run = 3;
      }
      for(int i = 0; i < run; ++i)
      {
        advance();
      }
      return read_string_body(quote, run, quote == '"');
    }

    // Called with the opening delimiter already consumed.
    std::string read_string_body(char quote, int run, bool escapes)
    {
      const bool  multi = run == 3;
      std::string out;

      // "A newline immediately following the opening delimiter will be
      // trimmed." Only the first one: the second is content.
      if(multi and at_line_end() and not at_end())
      {
        next_line();
      }

      while(true)
      {
        if(at_end())
        {
          throw std::runtime_error("TOML: unterminated string");
        }
        if(at_line_end())
        {
          if(not multi)
          {
            throw std::runtime_error("TOML: a line break cannot appear in a single-line string");
          }
          out.push_back('\n');
          next_line();
          continue;
        }

        const char c = peek_at(0);
        if(c == quote)
        {
          // "mlb-quotes = 1*2quotation-mark" then the three-quote delimiter: a
          // run of four ends the string with one quote of content, a run of five
          // with two. One or two anywhere else is ordinary content.
          int n = 0;
          while(n < 6 and peek_at(static_cast<std::size_t>(n)) == quote)
          {
            ++n;
          }
          if(not multi)
          {
            advance();
            return out;
          }
          if(n < 3)
          {
            for(int i = 0; i < n; ++i)
            {
              out.push_back(quote);
              advance();
            }
            continue;
          }
          if(n > 5)
          {
            throw std::runtime_error(
                "TOML: more than two quotes in a row inside a multi-line string");
          }
          out.append(static_cast<std::size_t>(n - 3), quote);
          for(int i = 0; i < n; ++i)
          {
            advance();
          }
          return out;
        }

        if(escapes and c == '\\')
        {
          advance();
          if(multi and eat_line_continuation_())
          {
            continue;
          }
          decode_escape_(out);
          continue;
        }

        // "basic-unescaped = wschar / %x21 / %x23-5B / %x5D-7E / non-ascii": tab
        // is in, every other control byte and DEL are out.
        if(c != '\t' and serde::detail::is_control(c))
        {
          throw std::runtime_error(
              "TOML: a control character other than tab cannot appear literally in a string");
        }
        out.push_back(c);
        advance();
      }
    }

    static constexpr bool ends_token(char c)
    {
      return c == ' ' or c == '\t' or c == ',' or c == ']' or c == '}' or c == '#';
    }

    // The view points into a buffer the next token overwrites.
    std::string_view read_token()
    {
      token_buf_.clear();
      scan_token_run_();
      // The one place a value token holds a space. Continued only when a time
      // really follows, so "1979-05-27 # today" still ends at the date.
      if(is_full_date(token_buf_) and peek_at(0) == ' ' and at_time_ahead_(1))
      {
        token_buf_.push_back(' ');
        advance();
        scan_token_run_();
      }
      return token_buf_;
    }

    // Shape alone: four digits and a dash open a date, two digits and a colon open
    // a time. Both are unambiguous against every numeric form.
    bool at_date_time()
    {
      if(not serde::detail::is_dec_digit(peek_at(0)) or not serde::detail::is_dec_digit(peek_at(1)))
      {
        return false;
      }
      if(peek_at(2) == ':')
      {
        return true;
      }
      return serde::detail::is_dec_digit(peek_at(2)) and serde::detail::is_dec_digit(peek_at(3))
         and peek_at(4) == '-';
    }

    // Shape recognition, not a calendar parse: the text is carried verbatim into a
    // string. Seconds are optional as of 1.1, so "07:32" is a local time.
    static bool is_full_date(std::string_view s)
    {
      return s.size() == 10 and all_digits(s.substr(0, 4)) and s[4] == '-'
         and all_digits(s.substr(5, 2)) and s[7] == '-' and all_digits(s.substr(8, 2));
    }

    static bool is_date_time(std::string_view s)
    {
      if(s.size() >= 10 and is_full_date(s.substr(0, 10)))
      {
        if(s.size() == 10)
        {
          return true; // a local date
        }
        const char delim = s[10];
        return (delim == 'T' or delim == 't' or delim == ' ') and is_partial_time(s.substr(11));
      }
      return is_partial_time(s);
    }

    // "partial-time = time-hour ':' time-minute [ ':' time-second ... ]". Only
    // the head is checked, an offset or a fractional second is the tail.
    static bool is_partial_time(std::string_view s)
    {
      return s.size() >= 5 and all_digits(s.substr(0, 2)) and s[2] == ':'
         and all_digits(s.substr(3, 2));
    }

    std::string read_text()
    {
      if(at_string())
      {
        return read_string();
      }
      if(at_date_time())
      {
        const std::string_view token = read_token();
        if(not is_date_time(token))
        {
          throw std::runtime_error(std::format("TOML: '{}' is not a date-time", token));
        }
        return std::string{token};
      }
      throw std::runtime_error(std::format("TOML: expected a string, got '{}'", read_token()));
    }

    // A bracketed, comma-separated run in 1.1's grammar, which is one rule for the
    // array and the inline table. The serializer still writes the one-line,
    // no-trailing-comma form both versions accept; do not make it match this.
    template <char Open, char Close, typename Elem>
    void read_bracketed(std::string_view what, Elem&& read_one)
    {
      // A value position, which stops a bare "key = value" line inside one from
      // being read as a document.
      const value_scope guard{*this};

      advance(); // Open, which the caller has already looked at
      if(not next_content_line())
      {
        throw std::runtime_error(std::format("TOML: unterminated {}", what));
      }
      if(peek_at(0) == Close)
      {
        advance();
        return;
      }
      while(true)
      {
        read_one();
        if(not next_content_line())
        {
          throw std::runtime_error(std::format("TOML: unterminated {}", what));
        }
        const char sep = advance();
        if(sep == Close)
        {
          return;
        }
        if(sep != ',')
        {
          throw std::runtime_error(
              std::format("TOML: expected ',' or '{}' in {}", Close, what));
        }
        if(not next_content_line())
        {
          throw std::runtime_error(std::format("TOML: unterminated {}", what));
        }
        if(peek_at(0) == Close)
        {
          advance(); // a trailing comma, which 1.1 allows in both forms
          return;
        }
      }
    }

    // The views point into key_buf_, and are built only once the buffer is final,
    // because appending a segment can reallocate it.
    std::size_t read_key_path(std::span<std::string_view> out)
    {
      return read_key_path_into(out, key_buf_);
    }

    // A [header] path has to outlive every assignment under it, so it cannot share
    // key_buf_.
    std::size_t read_key_path_into(std::span<std::string_view> out, std::string& buf)
    {
      buf.clear();
      std::array<std::size_t, serde::max_key_depth> ends{};
      std::size_t                                   count = 0;
      while(true)
      {
        skip_ws();
        if(count == out.size())
        {
          throw std::runtime_error("TOML: a key has more dotted segments than can be followed");
        }
        read_key_segment_(buf);
        ends[count++] = buf.size();
        skip_ws();
        if(at_line_end() or peek_at(0) != '.')
        {
          break;
        }
        advance(); // "dot-sep = ws %x2E ws"
      }
      std::size_t start = 0;
      for(std::size_t i = 0; i < count; ++i)
      {
        out[i] = std::string_view{buf}.substr(start, ends[i] - start);
        start  = ends[i];
      }
      return count;
    }

    template <typename Map> void read_inline_pair(Map& value)
    {
      std::array<std::string_view, serde::max_key_depth> keys{};
      const std::size_t count = read_key_path(keys);

      skip_ws();
      if(at_line_end() or peek_at(0) != '=')
      {
        throw std::runtime_error(
            std::format("TOML: expected '=' after the key '{}'", keys[count - 1]));
      }
      advance();
      if(not next_content_line())
      {
        throw std::runtime_error("TOML: a key has no value");
      }

      serde::object_visit(std::span{keys.data(), count}, value, [&]<typename V>(V& v) {
        v = this->template load<std::remove_cvref_t<V>>();
      });
    }

    // A document is flat. A [a.b.c] header re-targets every following assignment
    // at an arbitrary depth and the next may jump back to depth 1, so there is no
    // call for a recursive-descent reader to return from: this stays at the root
    // and addresses every assignment by its full path. Only reachable at depth 0.
    template <typename Root> void read_document(Root& root)
    {
      table_path_.clear();
      header_buf_.clear();
      paths_.clear();

      while(next_content_line())
      {
        if(peek_at(0) == '[')
        {
          read_header_();
        }
        else
        {
          read_assignment_(root);
        }
        // Leaves the cursor on the break, which next_content_line() eats on the
        // way round.
        finish_line();
      }
    }

  private:
    // The table every following assignment belongs to, until the next header.
    void read_header_()
    {
      advance(); // '['
      const bool array = not at_line_end() and peek_at(0) == '[';
      if(array)
      {
        advance();
      }

      std::array<std::string_view, serde::max_key_depth> names{};
      const std::size_t count = read_key_path_into(names, header_buf_);
      skip_ws();
      for(int i = 0; i < (array ? 2 : 1); ++i)
      {
        if(at_line_end() or peek_at(0) != ']')
        {
          throw std::runtime_error(std::format(
              "TOML: expected '{}' after the table header '{}'",
              array ? "]]" : "]",
              std::string_view{names[count - 1]}));
        }
        advance();
      }

      enter_header_(std::span<std::string_view const>{names.data(), count}, array);
    }

    // A prefix segment naming an [[array of tables]] addresses that array's
    // current element, so "[[a]] [a.sub]" reaches into the last a.
    void enter_header_(std::span<std::string_view const> names, bool array)
    {
      table_path_.clear();
      std::string key;

      for(std::size_t i = 0; i + 1 < names.size(); ++i)
      {
        detail::segment   seg{names[i], detail::no_index};
        const std::string plain = detail::with_segment(key, seg);
        const auto        it    = paths_.find(plain);
        if(it == paths_.end())
        {
          paths_.emplace(plain, detail::path_entry{detail::path_kind::implied, 0});
        }
        else if(it->second.kind == detail::path_kind::value)
        {
          throw std::runtime_error(std::format(
              "TOML: the header '{}' reaches into '{}', which an earlier assignment made a "
              "value: neither a scalar nor a table written inline can be extended by a header",
              header_text_(names, array), names[i]));
        }
        else if(it->second.kind == detail::path_kind::array)
        {
          seg.index = it->second.count - 1;
        }
        key = detail::with_segment(key, seg);
        table_path_.push_back(seg);
      }

      detail::segment   seg{names.back(), detail::no_index};
      const std::string plain = detail::with_segment(key, seg);
      if(array)
      {
        const auto [it, fresh] =
            paths_.try_emplace(plain, detail::path_entry{detail::path_kind::array, 0});
        if(not fresh and it->second.kind != detail::path_kind::array)
        {
          throw std::runtime_error(std::format(
              "TOML: '{}' names a path that is already defined as something other than an array "
              "of tables",
              header_text_(names, array)));
        }
        // "[[a]] ... [[b]] ... [[a]]" is legal and the second [[a]] is element
        // 1: the count is never reset, only ever appended to.
        seg.index = it->second.count++;
        key       = detail::with_segment(key, seg);
        paths_.emplace(key, detail::path_entry{detail::path_kind::header, 0});
      }
      else
      {
        const auto [it, fresh] =
            paths_.try_emplace(plain, detail::path_entry{detail::path_kind::header, 0});
        if(not fresh)
        {
          if(it->second.kind == detail::path_kind::array)
          {
            throw std::runtime_error(std::format(
                "TOML: the table '{}' names a path an earlier '[[{}]]' made an array of tables",
                header_text_(names, array), names.back()));
          }
          if(it->second.kind != detail::path_kind::implied)
          {
            throw std::runtime_error(std::format(
                "TOML: the table '{}' is defined twice", header_text_(names, array)));
          }
          // "[a.b]" and then "[a]": a super-table may be written after the
          // sub-table that implied it, once.
          it->second.kind = detail::path_kind::header;
        }
        key = plain;
      }
      table_path_.push_back(seg);
    }

    // The header as the document spelled it, for a message.
    static std::string header_text_(std::span<std::string_view const> names, bool array)
    {
      std::string out;
      for(std::string_view name : names)
      {
        if(not out.empty())
        {
          out += '.';
        }
        out += name;
      }
      return array ? std::format("[[{}]]", out) : std::format("[{}]", out);
    }

    // Both relative to the table the last header named.
    template <typename Root> void read_assignment_(Root& root)
    {
      std::array<std::string_view, serde::max_key_depth> keys{};
      const std::size_t                                  count = read_key_path(keys);
      skip_ws();
      if(at_line_end() or peek_at(0) != '=')
      {
        throw std::runtime_error(
            std::format("TOML: expected '=' after the key '{}'", keys[count - 1]));
      }
      advance();
      skip_ws();
      // "keyval-sep = ws %x3D ws", ws being space and tab only: a value starts on
      // the line its key is on, though a bracket it opens may run past it.
      if(at_line_end())
      {
        throw std::runtime_error(
            std::format("TOML: the key '{}' has no value", keys[count - 1]));
      }

      std::array<detail::segment, serde::max_key_depth> full{};
      if(table_path_.size() + count > full.size())
      {
        throw std::runtime_error("TOML: a key has more dotted segments than can be followed");
      }
      std::size_t n = 0;
      for(detail::segment seg : table_path_)
      {
        full[n++] = seg;
      }
      for(std::size_t i = 0; i < count; ++i)
      {
        full[n++] = detail::segment{keys[i], detail::no_index};
      }

      const auto path = std::span<detail::segment const>{full.data(), n};
      record_assignment_(path, table_path_.size());

      const std::string where = detail::join_path(path);
      auto              assign = [this]<typename V>(V& v) {
        const value_scope guard{*this};
        v = this->template load<std::remove_cvref_t<V>>();
      };
      try
      {
        detail::table_visit(path, root, assign, where);
      }
      catch(std::exception const& e)
      {
        rethrow_at_key_(e, where);
      }
    }

    // A message this backend wrote already names the format and the offending
    // text; anything from object_visit or a destination type names neither.
    [[noreturn]] static void rethrow_at_key_(std::exception const& e, std::string_view where)
    {
      if(std::string_view{e.what()}.starts_with("TOML"))
      {
        throw;
      }
      throw std::runtime_error(std::format("TOML: key '{}': {}", where, e.what()));
    }

    // TOML's define-it-once rules, all statements about paths_. See path_kind.
    void record_assignment_(std::span<detail::segment const> path, std::size_t table_count)
    {
      std::string key;
      for(std::size_t i = 0; i + 1 < path.size(); ++i)
      {
        key = detail::with_segment(key, path[i]);
        if(i < table_count)
        {
          // Set by a header, which recorded it when it read it.
          continue;
        }
        const auto [it, fresh] =
            paths_.try_emplace(key, detail::path_entry{detail::path_kind::dotted, 0});
        if(not fresh and it->second.kind != detail::path_kind::dotted)
        {
          throw std::runtime_error(std::format(
              "TOML: the dotted key '{}' would redefine '{}', which is already defined",
              detail::join_path(path), detail::join_path(path.first(i + 1))));
        }
      }
      key = detail::with_segment(key, path.back());
      const auto [it, fresh] =
          paths_.try_emplace(key, detail::path_entry{detail::path_kind::value, 0});
      (void)it;
      if(not fresh)
      {
        throw std::runtime_error(
            std::format("TOML: the key '{}' is defined twice", detail::join_path(path)));
      }
    }

    // "simple-key = quoted-key / unquoted-key", appended to `buf`.
    void read_key_segment_(std::string& buf)
    {
      if(at_line_end())
      {
        throw std::runtime_error("TOML: expected a key");
      }
      const char c = peek_at(0);
      if(c == '"' or c == '\'')
      {
        // "quoted-key = basic-string / literal-string": the multi-line forms
        // are not keys.
        if(peek_at(1) == c and peek_at(2) == c)
        {
          throw std::runtime_error("TOML: a multi-line string cannot be a key");
        }
        buf += read_string();
        return;
      }
      const std::size_t start = buf.size();
      while(not at_line_end() and detail::is_bare_key_char(peek_at(0)))
      {
        buf.push_back(advance());
      }
      if(buf.size() == start)
      {
        throw std::runtime_error(std::format("TOML: '{}' cannot start a key", c));
      }
    }

    void scan_token_run_()
    {
      while(not at_line_end() and not ends_token(peek_at(0)))
      {
        token_buf_.push_back(advance());
      }
    }

    bool at_time_ahead_(std::size_t at)
    {
      return serde::detail::is_dec_digit(peek_at(at))
         and serde::detail::is_dec_digit(peek_at(at + 1)) and peek_at(at + 2) == ':';
    }

    static bool all_digits(std::string_view s)
    {
      return std::ranges::all_of(s, serde::detail::is_dec_digit);
    }

    // "mlb-escaped-nl = escape ws newline *( wschar / newline )": a backslash ends
    // a line by eating the break and every space, tab and blank line after it.
    // Not yaml's folding, which turns a break into a space. Called with the
    // backslash consumed; answers false and consumes nothing for a real escape.
    bool eat_line_continuation_()
    {
      std::size_t k = 0;
      while(peek_at(k) == ' ' or peek_at(k) == '\t')
      {
        ++k;
      }
      if(not is_break(peek_at(k)))
      {
        return false;
      }
      for(std::size_t i = 0; i < k; ++i)
      {
        advance();
      }
      next_line();
      while(not at_end())
      {
        const char c = peek_at(0);
        if(c == ' ' or c == '\t')
        {
          advance();
          continue;
        }
        if(is_break(c))
        {
          next_line();
          continue;
        }
        break;
      }
      return true;
    }

    int hex_digits_(int count)
    {
      return serde::detail::decode_hex_escape(
          count,
          [this] {
            if(at_line_end())
            {
              throw std::runtime_error("TOML: truncated hexadecimal escape");
            }
            return advance();
          },
          [](char d) { return std::format("TOML: invalid hexadecimal escape digit: {}", d); });
    }

    // The 1.1 compact set is \b \t \n \f \r \e \" \\ - no \0, no \a, no \v, and
    // no \/ either, which JSON has and TOML does not.
    void decode_escape_(std::string& out)
    {
      if(at_line_end())
      {
        throw std::runtime_error("TOML: a line break cannot follow a backslash here");
      }
      const char esc = advance();
      switch(esc)
      {
        case '"':
        case '\\':
          out.push_back(esc);
          return;
        case 'b':
          out.push_back('\b');
          return;
        case 't':
          out.push_back('\t');
          return;
        case 'n':
          out.push_back('\n');
          return;
        case 'f':
          out.push_back('\f');
          return;
        case 'r':
          out.push_back('\r');
          return;
        case 'e':
          out.push_back('\x1B');
          return;
        case 'x':
        case 'u':
        case 'U':
        {
          // Only the subset below 0x80 is decoded, which is exactly what the
          // serializer emits; anything above needs a multi-byte encoding.
          const int count = esc == 'x' ? 2 : (esc == 'u' ? 4 : 8);
          const int code  = hex_digits_(count);
          if(code > 0x7F)
          {
            throw std::runtime_error(
                std::format("TOML: \\{} escapes above 0x7F are not implemented", esc));
          }
          out.push_back(static_cast<char>(code));
          return;
        }
        default:
          throw std::runtime_error(std::format("TOML: unknown escape: \\{}", esc));
      }
    }

  public:
    friend null_t tag_invoke(
        tag_t<serde::deserialize>,
        [[maybe_unused]] deserializer<InputIt>& de,
        std::type_identity<null_t>)
    {
      throw std::runtime_error("TOML has no null: there is no literal to read one from");
    }

    template <typename T>
    friend std::optional<T> tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<std::optional<T>>)
    {
      return de.template load<T>();
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<boolean>)
    {
      const std::string_view token = de.read_token();
      if(token == "true")
      {
        return true;
      }
      if(token == "false")
      {
        return false;
      }
      throw std::runtime_error(
          std::format("TOML: expected a boolean, got '{}'; true and false are lowercase", token));
    }

    template <number_c Num>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Num>)
    {
      if(de.at_date_time())
      {
        throw std::runtime_error(
            "TOML: a date-time is read as a string, not as a number: this backend keeps the four "
            "date-time types verbatim and maps none of them onto a calendar type");
      }
      const std::string_view token = de.read_token();
      // Grammar before parser: from_chars takes the leading zero of "0123" and the
      // bare digits behind a stripped "0x" prefix that TOML never signs.
      if(not(detail::matches_int<detail::toml_numbers>(token)
             or detail::matches_float<detail::toml_numbers>(token)))
      {
        throw std::runtime_error(std::format("TOML: '{}' is not a number", token));
      }
      return detail::parse_number<detail::toml_numbers, Num>(token, "TOML");
    }

    template <std::same_as<char> Char>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Char>)
    {
      const std::string text = de.read_text();
      if(text.size() != 1)
      {
        throw std::runtime_error(
            std::format("TOML: expected a single character, got '{}'", text));
      }
      return text.front();
    }

    // A separate overload rather than a static_assert inside the good one: the
    // assert alone does not stop the body instantiating.
    template <str_c Str>
      requires(not serde::detail::string_sink_c<Str>)
    friend Str
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
      if constexpr(serde::detail::borrowed_string_sink_c<Str>)
      {
        static_assert(
            false,
            std::string(display_string_of(dealias(^^Str)))
                + " cannot be a TOML string destination: this backend does not offer a borrowed"
                  " read, because three of the four string forms are decoded rather than pointed"
                  " at and the fourth can still span lines (use std::string)");
      }
      else
      {
        static_assert(
            false,
            std::string(display_string_of(dealias(^^Str)))
                + " cannot be a TOML string destination: it does not own writable storage"
                  " (expected std::string, reflex::heapless::string<N> or std::array<char, N>)");
      }
      std::unreachable();
    }

    template <str_c Str>
      requires serde::detail::string_sink_c<Str>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
      const std::string text = de.read_text();

      Str value{};
      if constexpr(requires { value.append(std::string_view{}); })
      {
        value.append(std::string_view{text});
      }
      else if constexpr(serde::detail::growable_string_sink_c<Str>)
      {
        for(const char c : text)
        {
          value.push_back(c);
        }
      }
      else
      {
        auto it = std::begin(value);
        for(const char c : text)
        {
          if(it == std::end(value))
          {
            throw std::out_of_range("String too long to fit in target type");
          }
          *it++ = c;
        }
      }
      return value;
    }

    template <derives_c<derive_t<Parse>> T>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<T>)
    {
      const std::string text   = de.read_text();
      auto              result = parse<std::remove_cvref_t<T>>(text);
      if(!result)
      {
        throw std::runtime_error(
            std::format(
                "Failed to parse value: {}", std::generic_category().message(int(result.error()))));
      }
      return std::move(result).value();
    }
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

  // An array of inline tables is valid TOML from anyone else and reads through
  // the element type like any other, and so does a mixed-element array.
  template <typename InputIt, seq_c Seq>
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Seq>)
  {
    if(de.at_end() or de.peek_at(0) != '[')
    {
      throw std::runtime_error("TOML: expected '[' at the start of an array");
    }

    Seq value{};
    using elem_type = typename std::remove_cvref_t<Seq>::value_type;
    auto push =
        serde::detail::make_pusher(value, "Array has more elements than target type can hold");

    de.template read_bracketed<'[', ']'>("an array", [&] {
      push(de.template load<elem_type>());
    });
    return value;
  }

  // An inline table and a whole document, told apart by the brace first and the
  // depth second: a table in a value position is written inline or is a [header]
  // elsewhere, so a braceless table is a document and only depth 0 begins one. A
  // top-level "{ ... }" is accepted because load<T>() is also how a caller reads
  // a bare value.
  template <typename InputIt, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char, N> may be considered a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Map>)
  {
    Map value{};
    if(not de.at_end() and de.peek_at(0) == '{')
    {
      de.template read_bracketed<'{', '}'>("an inline table", [&] { de.read_inline_pair(value); });
      return value;
    }
    if(not de.at_document_level())
    {
      throw std::runtime_error(
          "TOML: expected '{' at the start of an inline table: a table in a value position is "
          "written inline, and a [header] cannot appear here");
    }
    de.read_document(value);
    return value;
  }

} // namespace reflex::serde::toml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto toml = ^^reflex::serde::toml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto toml = ^^reflex::serde::toml::deserializer;
}
