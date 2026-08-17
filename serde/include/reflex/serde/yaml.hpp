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

#include <reflex/serde/detail/escape.hpp>
#include <reflex/serde/detail/io.hpp>
#include <reflex/serde/detail/line_cursor.hpp>
#include <reflex/serde/detail/text.hpp>

REFLEX_EXPORT namespace reflex::serde::yaml
{
  namespace detail
  {
  using serde::detail::iequals;
  using serde::detail::iequals_any;
  using serde::detail::is_control;
  using serde::detail::matches_float;
  using serde::detail::matches_int;
  using serde::detail::parse_number;
  using serde::detail::string_view_of;
  using serde::detail::yaml_numbers;

  // A plain scalar may not begin with one of these. '-', '?' and ':' are absent:
  // they only bar a plain scalar when a space or the end of the scalar follows,
  // and that is tested separately.
  constexpr bool is_indicator(char c)
  {
    return c == ',' or c == '[' or c == ']' or c == '{' or c == '}' or c == '#' or c == '&'
        or c == '*' or c == '!' or c == '|' or c == '>' or c == '\'' or c == '"' or c == '%'
        or c == '@' or c == '`';
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
    return matches_int<yaml_numbers>(s) or matches_float<yaml_numbers>(s);
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

  // The quote character stays open, write_scalar picks between the two.
  using escapes = serde::detail::yaml_escapes;

  template <char Quote, typename Ser> void write_backslash_quoted(Ser& ser, std::string_view text)
  {
    serde::detail::write_backslash_quoted<escapes, Quote>(ser, text);
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
      serde::detail::write_doubled_quoted<'\''>(ser, text);
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

  // Where a plain scalar ends. In block context it runs to the end of the line;
  // inside a flow collection the flow indicators terminate it too.
  //
  // JSON bakes its terminator set into a constant (json.hpp:237-240). YAML
  // cannot: "version: 1.2.3-rc1" is one plain scalar in block context, and
  // "[1.2.3-rc1]" is one in flow context, and applying the flow set in block
  // context truncates the first at the dot.
  enum class scan_context
  {
    block,
    flow,
  };

  constexpr bool is_flow_indicator(char c)
  {
    return c == ',' or c == '[' or c == ']' or c == '{' or c == '}';
  }

  constexpr bool is_null_text(std::string_view s)
  {
    return s.empty() or s == "~" or s == "null" or s == "Null" or s == "NULL";
  }

  // A scalar as read, plus whether it was quoted or came from a block header.
  //
  // The flag is what makes the round trip work: quoting defeats resolution, so
  // '42' is the string "42" and 42 is the number. Step 10 turns on it.
  struct scalar
  {
    std::string text;
    bool        quoted = false;
  };
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
  class deserializer : public serde::detail::line_cursor<InputIt>
  {
    using base = serde::detail::line_cursor<InputIt>;

    // Backs read_key() when the key is quoted or needs trimming, which on this
    // backend is every key: a scalar is decoded rather than pointed at. One
    // buffer, not a pool - a key is consumed by object_visit_flat and dead
    // before the next one is read.
    std::string key_buf_;

    // A "---" has been seen, or content has started. Either way a second one is
    // a second document.
    bool doc_started_ = false;

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

    static constexpr std::string_view format_name = "YAML";

    static constexpr std::size_t npos = std::size_t(-1);

    // makes load() without an explicit type read a yaml::value
    using default_load_type = yaml::value;

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
      return column();
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
      if(column() == 0)
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
        return column(); // already parked on a node
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
      if(column() == 0 and marker_here_())
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
      return column();
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

  public:
    // deserialize(de) with no destination type. default_load_type covers
    // load(), but a caller reaching the CPO directly - reflex-serde-convert
    // does - needs this too.
    friend auto tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de)
    {
      return deserialize(de, std::type_identity<yaml::value>{});
    }

    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<null_t>)
    {
      const auto s = de.read_scalar();
      if(s.quoted or not detail::is_null_text(s.text))
      {
        throw std::runtime_error(std::format("YAML: expected a null, got '{}'", s.text));
      }
      return null;
    }

    template <typename T>
    friend std::optional<T> tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<std::optional<T>>)
    {
      // Nothing after the colon does not mean null on its own: the value may be
      // a block on the following lines. Only the indent of the next node tells
      // an absent value from a nested one, and next_content_line() is
      // idempotent so asking does not disturb the enclosing loop.
      if(de.at_line_end())
      {
        const std::size_t next = de.next_content_line();
        if(next == deserializer<InputIt>::npos or not de.deeper_than_block(next))
        {
          return std::nullopt;
        }
        return de.template load<T>();
      }
      if(de.at_null())
      {
        de.read_scalar();
        return std::nullopt;
      }
      return de.template load<T>();
    }

    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<boolean>)
    {
      const auto s = de.read_scalar();
      if(s.text == "true" or s.text == "True" or s.text == "TRUE")
      {
        return true;
      }
      if(s.text == "false" or s.text == "False" or s.text == "FALSE")
      {
        return false;
      }
      // Accepting these would make the value depend on which YAML version the
      // reader implements. The serializer quotes them for the same reason, so
      // this library never writes what it then refuses.
      if(detail::iequals_any(s.text, {"yes", "no", "on", "off", "y", "n"}))
      {
        throw std::runtime_error(
            std::format(
                "YAML: '{}' is a boolean in YAML 1.1 but a string in 1.2; write true or false",
                s.text));
      }
      throw std::runtime_error(std::format("YAML: expected a boolean, got '{}'", s.text));
    }

    template <number_c Num>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Num>)
    {
      return detail::parse_number<detail::yaml_numbers, Num>(de.read_scalar().text, "YAML");
    }

    template <std::same_as<char> Char>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Char>)
    {
      const auto s = de.read_scalar();
      if(s.text.size() != 1)
      {
        throw std::runtime_error(
            std::format("YAML: expected a single character, got '{}'", s.text));
      }
      return s.text.front();
    }

    // A string destination that neither owns writable storage nor can be
    // pointed at the input.
    //
    // Refused here rather than left to the fill path below, which is written
    // against iterators and would accept a std::string_view syntactically and
    // then fail on the assignment with a message naming neither the type nor
    // the member. A separate overload rather than a static_assert inside the
    // good one: the assert alone does not stop the body instantiating, so the
    // original error still follows it.
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
                + " cannot be a YAML string destination: this backend does not offer a borrowed"
                  " read, because a scalar is decoded rather than pointed at (use std::string)");
      }
      else
      {
        static_assert(
            false,
            std::string(display_string_of(dealias(^^Str)))
                + " cannot be a YAML string destination: it does not own writable storage"
                  " (expected std::string, reflex::heapless::string<N> or std::array<char, N>)");
      }
      std::unreachable();
    }

    template <str_c Str>
      requires serde::detail::string_sink_c<Str>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<Str>)
    {
      const auto s = de.read_scalar();

      Str value{};
      if constexpr(requires { value.append(std::string_view{}); })
      {
        value.append(std::string_view{s.text});
      }
      else if constexpr(serde::detail::growable_string_sink_c<Str>)
      {
        for(const char c : s.text)
        {
          value.push_back(c);
        }
      }
      else
      {
        auto it = std::begin(value);
        for(const char c : s.text)
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

    // What a poly::var holds is decided by syntax where syntax can decide it,
    // and by the scalar's own text otherwise.
    //
    // JSON settles this on one byte (json.hpp:1021-1045) because its grammar
    // makes the first byte decisive. YAML's does not: 'n' starts both null and
    // name, a block mapping starts with whatever its first key starts with, and
    // '-' starts both a sequence entry and a negative number. So the structural
    // forms are tested first, each with the lookahead it needs, and everything
    // else is read as a scalar and classified afterwards.
    template <typename var_type>
      requires(meta::is_template_instance_of(^^var_type, ^^poly::var))
    friend auto tag_invoke(
        tag_t<serde::deserialize>,
        deserializer<InputIt>& de,
        std::type_identity<var_type>) -> var_type
    {
      const std::size_t indent = de.next_content_line();
      if(indent == deserializer<InputIt>::npos or not de.deeper_than_block(indent))
      {
        return var_type{}; // an absent value; the cursor stays parked
      }

      const char c = de.peek_at(0);
      if(c == '[')
      {
        return de.template load<typename var_type::arr_type>();
      }
      if(c == '{')
      {
        return de.template load<typename var_type::obj_type>();
      }
      if(de.at_sequence_entry())
      {
        return de.template load<typename var_type::arr_type>();
      }
      if(de.at_block_mapping())
      {
        return de.template load<typename var_type::obj_type>();
      }
      return de.template resolve_scalar<var_type>();
    }

    template <derives_c<derive_t<Parse>> T>
    friend auto
        tag_invoke(tag_t<serde::deserialize>, deserializer<InputIt>& de, std::type_identity<T>)
    {
      const auto s      = de.read_scalar();
      auto       result = parse<std::remove_cvref_t<T>>(s.text);
      if(!result)
      {
        throw std::runtime_error(
            std::format(
                "Failed to parse value: {}", std::generic_category().message(int(result.error()))));
      }
      return std::move(result).value();
    }

  private:
    detail::scan_context ctx_ = detail::scan_context::block;

    // The indent of the block being parsed. A child block must start strictly
    // deeper. no_block is the top level, where anything is deep enough - it
    // cannot be 0, or a top-level mapping at column 0 would reject itself.
    std::size_t block_indent_ = no_block;

  public:
    static constexpr std::size_t no_block = std::size_t(-1);

    bool deeper_than_block(std::size_t indent) const
    {
      return block_indent_ == no_block or indent > block_indent_;
    }

    std::size_t block_indent() const
    {
      return block_indent_;
    }

    // Sets the indent of the block being parsed for the duration, restoring the
    // enclosing one on the way out. RAII so a throwing member cannot leave a
    // stale indent behind for the rest of the document.
    class indent_scope
    {
      deserializer* d_;
      std::size_t   saved_;

    public:
      indent_scope(deserializer& d, std::size_t indent) : d_{&d}, saved_{d.block_indent_}
      {
        d_->block_indent_ = indent;
      }
      indent_scope(indent_scope const&)            = delete;
      indent_scope& operator=(indent_scope const&) = delete;
      ~indent_scope()
      {
        d_->block_indent_ = saved_;
      }
    };

    [[nodiscard]] indent_scope enter_block(std::size_t indent)
    {
      return indent_scope{*this, indent};
    }

    // Inside a flow collection there is no block structure: whitespace, line
    // breaks and comments are all insignificant, and a plain scalar gains the
    // flow indicators as terminators.
    //
    // Restores the enclosing block indent as well as the context, so a block
    // node appearing after a flow one does not inherit a neutralised indent.
    class flow_scope
    {
      deserializer*        d_;
      detail::scan_context saved_ctx_;
      std::size_t          saved_indent_;

    public:
      explicit flow_scope(deserializer& d)
          : d_{&d}, saved_ctx_{d.ctx_}, saved_indent_{d.block_indent_}
      {
        d_->ctx_          = detail::scan_context::flow;
        d_->block_indent_ = no_block;
      }
      flow_scope(flow_scope const&)            = delete;
      flow_scope& operator=(flow_scope const&) = delete;
      ~flow_scope()
      {
        d_->ctx_          = saved_ctx_;
        d_->block_indent_ = saved_indent_;
      }
    };

    [[nodiscard]] flow_scope enter_flow()
    {
      return flow_scope{*this};
    }

    // Whitespace, line breaks and comments, all of which a flow collection may
    // contain freely. The line breaks go through next_line() like every other
    // one, so column_ stays honest - this is the only place outside next_line()
    // that crosses a line, and it is the exception the cursor's newline rule
    // allows for.
    void skip_flow_space()
    {
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
        if(c == '#')
        {
          skip_to_line_end();
          continue;
        }
        break;
      }
    }

    // "[a, b]". A trailing comma before the bracket is legal in YAML, unlike in
    // JSON, which is why the loop re-tests for the closer after a separator.
    template <typename Seq> Seq read_flow_sequence()
    {
      advance(); // '['
      auto scope = enter_flow();

      Seq value{};
      using elem_type = typename std::remove_cvref_t<Seq>::value_type;

      auto push = serde::detail::make_pusher(
          value, "Sequence has more elements than target type can hold");

      skip_flow_space();
      if(at_end())
      {
        throw std::runtime_error("YAML: unterminated flow sequence");
      }
      if(peek_at(0) == ']')
      {
        advance();
        return value;
      }

      while(true)
      {
        push(this->template load<elem_type>());
        skip_flow_space();
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated flow sequence");
        }
        const char sep = advance();
        if(sep == ']')
        {
          break;
        }
        if(sep != ',')
        {
          throw std::runtime_error("YAML: expected ',' or ']' in a flow sequence");
        }
        skip_flow_space();
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated flow sequence");
        }
        if(peek_at(0) == ']')
        {
          advance(); // a trailing comma
          break;
        }
      }
      return value;
    }

    // "{a: 1, b: 2}", which is also every JSON object.
    template <typename Map> Map read_flow_mapping()
    {
      advance(); // '{'
      auto scope = enter_flow();

      Map value{};

      skip_flow_space();
      if(at_end())
      {
        throw std::runtime_error("YAML: unterminated flow mapping");
      }
      if(peek_at(0) == '}')
      {
        advance();
        return value;
      }

      while(true)
      {
        skip_flow_space();
        const std::string_view key = read_key();
        skip_flow_space();
        if(at_end() or peek_at(0) != ':')
        {
          throw std::runtime_error(std::format("YAML: expected ':' after the key '{}'", key));
        }
        advance();
        skip_flow_space();

        serde::object_visit_flat(key, value, [&]<typename V>(V& v) {
          v = this->template load<std::remove_cvref_t<V>>();
        });

        skip_flow_space();
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated flow mapping");
        }
        const char sep = advance();
        if(sep == '}')
        {
          break;
        }
        if(sep != ',')
        {
          throw std::runtime_error("YAML: expected ',' or '}' in a flow mapping");
        }
        skip_flow_space();
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated flow mapping");
        }
        if(peek_at(0) == '}')
        {
          advance(); // a trailing comma
          break;
        }
      }
      return value;
    }

    // A "- " opening a block sequence entry, as opposed to a '-' starting a
    // negative number. The space is the whole difference, which is why this
    // needs the lookahead buffer.
    //
    // "---" is not one: the second '-' fails the test.
    bool at_sequence_entry()
    {
      if(at_line_end() or peek_at(0) != '-')
      {
        return false;
      }
      const char next = peek_at(1);
      return next == '\0' or next == ' ' or next == '\t' or is_break(next);
    }

    // Does the current line open a block mapping?
    //
    // The one place the parser genuinely looks ahead rather than deciding on a
    // byte. It has to: a block mapping starts with whatever its first key
    // starts with, so only an unquoted ':' followed by a space or the line end
    // distinguishes "a: 1" from the plain scalar "a 1".
    //
    // Bounded by the line. An unbounded scan for a byte that may not be there
    // rescans to end of input on every call and turns a linear parse quadratic
    // - the same reasoning as detail/io.hpp:343-346.
    bool at_block_mapping()
    {
      bool in_single = false;
      bool in_double = false;

      for(std::size_t i = 0;; ++i)
      {
        const char c = peek_at(i);
        if(c == '\0' or is_break(c))
        {
          return false;
        }
        if(in_single)
        {
          // A doubled '' closes and immediately reopens, which lands on the
          // same answer as treating it as an escape.
          if(c == '\'')
          {
            in_single = false;
          }
          continue;
        }
        if(in_double)
        {
          if(c == '\\')
          {
            ++i; // whatever follows is escaped, including a quote
            continue;
          }
          if(c == '"')
          {
            in_double = false;
          }
          continue;
        }
        if(c == '\'')
        {
          in_single = true;
          continue;
        }
        if(c == '"')
        {
          in_double = true;
          continue;
        }
        if(c == '#' and i > 0 and (peek_at(i - 1) == ' ' or peek_at(i - 1) == '\t'))
        {
          return false; // the rest of the line is a comment
        }
        if(c == ':')
        {
          const char next = peek_at(i + 1);
          if(next == '\0' or next == ' ' or next == '\t' or is_break(next))
          {
            return true;
          }
        }
      }
    }

    // A scalar, classified against the YAML 1.2 core schema.
    //
    // Quoting defeats resolution: '42' is the string and 42 is the number. That
    // one rule is what makes a poly::var round trip, and it is why read_scalar
    // reports whether the scalar was quoted.
    //
    // The number shapes are matched with the same constexpr predicates the
    // serializer's quoting decision uses, so the writer cannot decide a scalar
    // is safe to leave plain while the reader disagrees about what it is.
    template <typename var_type> var_type resolve_scalar()
    {
      const detail::scalar s = read_scalar();

      if(s.quoted)
      {
        return yaml::string{s.text};
      }
      if(detail::is_null_text(s.text))
      {
        return null;
      }
      if(s.text == "true" or s.text == "True" or s.text == "TRUE")
      {
        return yaml::boolean{true};
      }
      if(s.text == "false" or s.text == "False" or s.text == "FALSE")
      {
        return yaml::boolean{false};
      }
      // yes/no/on/off deliberately fall through to string, which is what YAML
      // 1.2 core says they are. That differs from load<bool>(), which throws on
      // them: asked for a bool the intent is unambiguous and a 1.1 spelling is
      // worth an error, but asked what this *is*, 1.2 answers "a string".
      // The hex and octal forms have to go through the integral parser and then
      // widen: yaml::number is a double, and the floating-point branch of
      // parse_number has no notion of a base prefix. Plain decimal integers
      // need no such detour, std::from_chars reads them as a double directly.
      if(s.text.starts_with("0x") or s.text.starts_with("0o"))
      {
        return static_cast<yaml::number>(
            detail::parse_number<detail::yaml_numbers, long long>(s.text, "YAML"));
      }
      if(detail::matches_int<detail::yaml_numbers>(s.text)
         or detail::matches_float<detail::yaml_numbers>(s.text))
      {
        return detail::parse_number<detail::yaml_numbers, yaml::number>(s.text, "YAML");
      }
      return yaml::string{s.text};
    }

    // A mapping key, up to but not including its ':'.
    //
    // The view points into key_buf_ and stays valid only until the next call.
    // That is enough: object_visit_flat compares the key and copies whatever it
    // needs before it invokes the callback, so the value reader - which may
    // read another key - never runs while this one is still needed.
    //
    // A plain key may not contain a ':' at all, which is why the terminator
    // here is the bare colon rather than the ": " a plain *value* stops at.
    std::string_view read_key()
    {
      const char c = peek();
      if(c == '\'')
      {
        key_buf_ = read_single_quoted();
        return key_buf_;
      }
      if(c == '"')
      {
        key_buf_ = read_double_quoted();
        return key_buf_;
      }

      key_buf_.clear();
      while(not at_line_end())
      {
        const char ch = peek_at(0);
        if(ch == ':')
        {
          break;
        }
        if(ctx_ == detail::scan_context::flow and detail::is_flow_indicator(ch))
        {
          break;
        }
        key_buf_.push_back(ch);
        advance();
      }
      while(not key_buf_.empty() and (key_buf_.back() == ' ' or key_buf_.back() == '\t'))
      {
        key_buf_.pop_back();
      }
      return key_buf_;
    }

    // Spaces and tabs within a line. A tab may separate a key from its value
    // even though it may not indent a node, so this is not skip_indent().
    void skip_separation()
    {
      while(not at_line_end() and (peek_at(0) == ' ' or peek_at(0) == '\t'))
      {
        advance();
      }
    }

    // A scalar, whichever of the five forms it is written in.
    //
    // The caller has already skipped the separation whitespace. An empty value
    // - nothing between the colon and the line end - is a null, and comes back
    // as unquoted empty text so that resolution turns it into one.
    detail::scalar read_scalar()
    {
      if(at_line_end())
      {
        // Nothing after the colon is usually a null, but a plain scalar whose
        // first line is empty looks identical until the next content line is
        // examined: "key:" / "  a" / "  b" is "a b" in real YAML, neither a null
        // nor a nested block. Three answers from one lookahead, and the two this
        // reader declines leave the cursor parked where the enclosing loop
        // expects it, because next_content_line() is idempotent there.
        if(ctx_ == detail::scan_context::block and starts_plain_continuation_())
        {
          return {read_plain(), false};
        }
        return {};
      }
      switch(peek_at(0))
      {
        case '\'':
          return {read_single_quoted(), true};
        case '"':
          return {read_double_quoted(), true};
        case '|':
        case '>':
          return {read_block_scalar(peek_at(0)), true};
        default:
          return {read_plain(), false};
      }
    }

    // A plain scalar, folded across every line it continues onto.
    //
    // A continuation line has to be indented deeper than the enclosing block,
    // and a line that looks like a mapping is the only thing that stops it: "  b:
    // c" under "key: a" cannot be told from a nested block, so real YAML rejects
    // it and so does this - by parking on the line and letting the enclosing
    // block raise its over-indentation error. "  - b" is NOT ambiguous, it folds
    // into the text as "a - b", which is why at_sequence_entry() is not one of
    // the conditions here. Both measured against PyYAML rather than read off the
    // spec.
    //
    // Flow context is excluded. It folds too in real YAML, but block_indent_ is
    // neutralised there, so the indent test that bounds this loop has nothing to
    // say and there is no other bound to put in its place.
    std::string read_plain()
    {
      std::string first = read_plain_line_();
      if(ctx_ != detail::scan_context::block or not at_line_end())
      {
        return first; // a ": " or a comment ended it before the line did
      }

      // Empty until a continuation is actually found, so the single-line case -
      // which is nearly every scalar - allocates nothing beyond `first`.
      std::vector<std::string> lines;
      std::size_t              blanks = 0;
      while(next_line())
      {
        const std::size_t indent = skip_indent();
        if(at_end())
        {
          break;
        }
        if(at_line_end())
        {
          ++blanks; // a blank or whitespace-only line keeps its break
          continue;
        }
        // Stops ON the first line this scalar does not own, with the column
        // correct, so next_content_line()'s idempotence lets the enclosing loop
        // pick it up instead of skipping it. deeper_than_block() is tested
        // before at_block_mapping() on purpose: the common case is a sibling at
        // the same indent, and that must not pay for a line scan.
        if(peek_at(0) == '#' or not deeper_than_block(indent)
           or (column() == 0 and marker_here_()) or at_block_mapping())
        {
          break;
        }
        if(lines.empty())
        {
          lines.push_back(std::move(first));
        }
        lines.insert(lines.end(), blanks, std::string{});
        blanks = 0;
        lines.push_back(read_plain_line_());
        if(not at_line_end())
        {
          break;
        }
      }
      return lines.empty() ? std::move(first) : fold_lines_(lines);
    }

    // One line of a plain scalar: to the end of the line, or to ": ", or to
    // " #", or - in flow context only - to a flow indicator. Trailing
    // whitespace before the terminator is not part of the scalar.
    //
    // A fresh string per line rather than appending into a shared one, because
    // the " #" test looks at the byte before the '#' and the previous line's
    // last byte is not it.
    std::string read_plain_line_()
    {
      std::string out;

      // Bulk path, and the only one in the parser. Everything else reads a byte
      // at a time so that both cursor types run the same code; this one is here
      // because it measured worth the second path - 31% off a scalar-heavy
      // document and 16% off a deeply nested one. It needs a contiguous input
      // with nothing buffered, so the streaming cursor falls through to the
      // loop below and the two must agree. Every parser test runs on both.
      if constexpr(base::bulk_scan)
      {
        if(this->nothing_buffered())
        {
          const std::string_view sv = this->rest();

          // One pass, bounded by the line: a break ends the scan, so this
          // cannot run off into the rest of the document.
          std::size_t n = sv.size();
          for(std::size_t i = 0; i < sv.size(); ++i)
          {
            const char c = sv[i];
            if(is_break(c))
            {
              n = i;
              break;
            }
            if(c == ':')
            {
              const char next = (i + 1 < sv.size()) ? sv[i + 1] : '\0';
              if(next == '\0' or next == ' ' or next == '\t' or is_break(next)
                 or (ctx_ == detail::scan_context::flow and detail::is_flow_indicator(next)))
              {
                n = i;
                break;
              }
            }
            if(c == '#' and i > 0 and (sv[i - 1] == ' ' or sv[i - 1] == '\t'))
            {
              n = i;
              break;
            }
            if(ctx_ == detail::scan_context::flow and detail::is_flow_indicator(c))
            {
              n = i;
              break;
            }
          }

          out.assign(sv.substr(0, n));
          this->skip_in_line(n);
          while(not out.empty() and (out.back() == ' ' or out.back() == '\t'))
          {
            out.pop_back();
          }
          return out;
        }
      }

      while(not at_line_end())
      {
        const char c = peek_at(0);
        if(c == ':')
        {
          const char next = peek_at(1);
          if(next == '\0' or next == ' ' or next == '\t' or is_break(next)
             or (ctx_ == detail::scan_context::flow and detail::is_flow_indicator(next)))
          {
            break;
          }
        }
        // A '#' opens a comment only after whitespace. "http://x#y" is one URL.
        if(c == '#' and not out.empty() and (out.back() == ' ' or out.back() == '\t'))
        {
          break;
        }
        if(ctx_ == detail::scan_context::flow and detail::is_flow_indicator(c))
        {
          break;
        }
        out.push_back(c);
        advance();
      }
      while(not out.empty() and (out.back() == ' ' or out.back() == '\t'))
      {
        out.pop_back();
      }
      return out;
    }

    // Single-quoted: no backslash is special and the only escape is '' for a
    // quote.
    std::string read_single_quoted()
    {
      advance(); // the opening quote
      std::string out;
      while(true)
      {
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated single-quoted scalar");
        }
        if(at_line_end())
        {
          throw std::runtime_error("YAML: a multi-line single-quoted scalar is not supported");
        }
        const char c = advance();
        if(c == '\'')
        {
          if(peek_at(0) == '\'')
          {
            advance();
            out.push_back('\'');
            continue;
          }
          return out;
        }
        out.push_back(c);
      }
    }

    std::string read_double_quoted()
    {
      advance(); // the opening quote
      std::string out;
      while(true)
      {
        if(at_end())
        {
          throw std::runtime_error("YAML: unterminated double-quoted scalar");
        }
        if(at_line_end())
        {
          throw std::runtime_error("YAML: a multi-line double-quoted scalar is not supported");
        }
        const char c = advance();
        if(c == '"')
        {
          return out;
        }
        if(c != '\\')
        {
          out.push_back(c);
          continue;
        }
        if(at_line_end())
        {
          throw std::runtime_error("YAML: a line break cannot follow a backslash here");
        }
        decode_escape_(out);
      }
    }

    // A '|' or '>' block scalar, header and body.
    //
    // Stops on the first line whose indent is below the body's, WITHOUT
    // consuming it: the cursor is left on that line's first byte with the
    // column correct, which is exactly the position next_content_line() treats
    // as already-parked.
    std::string read_block_scalar(char style)
    {
      advance(); // '|' or '>'

      std::size_t explicit_indent = 0;
      char        chomp           = ' '; // clip
      while(not at_line_end())
      {
        const char c = peek_at(0);
        if(c >= '1' and c <= '9')
        {
          explicit_indent = static_cast<std::size_t>(c - '0');
          advance();
          continue;
        }
        if(c == '-' or c == '+')
        {
          chomp = c;
          advance();
          continue;
        }
        break;
      }
      finish_line(); // only spaces or a comment may follow the header

      const std::size_t base = (block_indent_ == no_block) ? 0 : block_indent_;
      std::size_t       body_indent =
          explicit_indent != 0 ? base + explicit_indent : std::string_view::npos;

      std::vector<std::string> lines;
      while(true)
      {
        if(not next_line())
        {
          break;
        }
        // A break at the very end of the input terminates the line before it,
        // it does not open an empty one after it. Without this a document
        // ending in a newline gains a blank body line, which "|+" then keeps.
        if(at_end())
        {
          break;
        }
        std::size_t indent = 0;
        while(not at_end() and peek_at(0) == ' ')
        {
          advance();
          ++indent;
        }
        if(at_line_end())
        {
          lines.emplace_back(); // a blank line is body content until proven otherwise
          continue;
        }
        if(body_indent == std::string_view::npos)
        {
          if(not deeper_than_block(indent))
          {
            break; // the block is empty
          }
          body_indent = indent;
        }
        if(indent < body_indent)
        {
          break;
        }
        // Indentation beyond the body's own is content.
        std::string line(indent - body_indent, ' ');
        while(not at_line_end())
        {
          line.push_back(advance());
        }
        lines.push_back(std::move(line));
      }

      // Trailing blank lines are the chomping indicator's business, not the
      // body's.
      std::size_t trailing = 0;
      while(not lines.empty() and lines.back().empty())
      {
        lines.pop_back();
        ++trailing;
      }

      std::string out;
      if(style == '|')
      {
        for(auto const& line : lines)
        {
          out += line;
          out.push_back('\n');
        }
      }
      else
      {
        out = fold_lines_(lines);
        if(not out.empty())
        {
          out.push_back('\n');
        }
      }
      out.append(trailing, '\n');

      if(chomp == '-')
      {
        while(out.ends_with('\n'))
        {
          out.pop_back();
        }
      }
      else if(chomp == ' ')
      {
        while(out.ends_with('\n'))
        {
          out.pop_back();
        }
        if(not out.empty())
        {
          out.push_back('\n');
        }
      }
      return out;
    }

    // True when the value about to be read is a null: an empty value, '~', or
    // one of the "null" spellings. Decided without consuming, so an optional
    // can fall through to its element type.
    bool at_null()
    {
      if(at_line_end())
      {
        return true;
      }
      const char c = peek_at(0);
      if(c == '~')
      {
        return terminates_token_(peek_at(1));
      }
      if(c != 'n' and c != 'N')
      {
        return false;
      }
      const char c1 = peek_at(1);
      const char c2 = peek_at(2);
      const char c3 = peek_at(3);
      const bool spelled =
          (c == 'n' and c1 == 'u' and c2 == 'l' and c3 == 'l')     // null
          or (c == 'N' and c1 == 'u' and c2 == 'l' and c3 == 'l')  // Null
          or (c == 'N' and c1 == 'U' and c2 == 'L' and c3 == 'L'); // NULL
      return spelled and terminates_token_(peek_at(4));
    }

  private:
    // Folds line breaks the way YAML folds them: a single break between two
    // non-empty lines becomes a space, and n blank lines between them keep n
    // breaks, the folded one included. Shared by the '>' block scalar and the
    // multi-line plain scalar, which fold identically - all that differs is what
    // each does with the break that ends the last line.
    static std::string fold_lines_(std::vector<std::string> const& lines)
    {
      std::string out;
      bool        first  = true;
      std::size_t blanks = 0;
      for(auto const& line : lines)
      {
        if(line.empty())
        {
          ++blanks;
          continue;
        }
        if(first)
        {
          first = false;
        }
        else if(blanks == 0)
        {
          out.push_back(' ');
        }
        else
        {
          out.append(blanks, '\n');
        }
        out += line;
        blanks = 0;
      }
      return out;
    }

    // Does a plain scalar begin on the next content line, deeper than the
    // enclosing block? Consumes up to that line, which is what makes it usable
    // only where the caller is prepared to park there.
    //
    // Every other node form answers no and is left to the reader that owns it: a
    // quoted or block scalar, a flow collection, an anchor, a tag. "- " and "? "
    // are structure here too, unlike on a continuation line where a plain scalar
    // is already running and both are ordinary text.
    bool starts_plain_continuation_()
    {
      const std::size_t indent = next_content_line();
      if(indent == npos or not deeper_than_block(indent))
      {
        return false;
      }
      const char c = peek_at(0);
      if(detail::is_indicator(c) or at_sequence_entry() or at_block_mapping())
      {
        return false;
      }
      return not(c == '?' and terminates_token_(peek_at(1)));
    }

    bool terminates_token_(char c) const
    {
      if(c == '\0' or c == ' ' or c == '\t' or is_break(c))
      {
        return true;
      }
      return ctx_ == detail::scan_context::flow and detail::is_flow_indicator(c);
    }

    int hex_digits_(int count)
    {
      return serde::detail::decode_hex_escape(
          count,
          [this] {
            if(at_line_end())
            {
              throw std::runtime_error("YAML: truncated hexadecimal escape");
            }
            return advance();
          },
          [](char d) {
            return std::format("YAML: invalid hexadecimal escape digit: {}", d);
          });
    }

    void decode_escape_(std::string& out)
    {
      const char esc = advance();
      switch(esc)
      {
        case '0':
          out.push_back('\0');
          return;
        case 'a':
          out.push_back('\a');
          return;
        case 'b':
          out.push_back('\b');
          return;
        case 't':
        case '\t':
          out.push_back('\t');
          return;
        case 'n':
          out.push_back('\n');
          return;
        case 'v':
          out.push_back('\v');
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
        case ' ':
        case '"':
        case '/':
        case '\\':
          out.push_back(esc);
          return;
        case 'N': // NEL, U+0085
        case '_': // NBSP, U+00A0
        case 'L': // LS, U+2028
        case 'P': // PS, U+2029
          throw std::runtime_error(
              std::format("YAML: the \\{} escape needs a multi-byte encoding, not implemented", esc));
        case 'x':
        case 'u':
        case 'U':
        {
          // Only the subset below 0x80 is decoded, which is exactly what the
          // serializer emits. Anything above would need a multi-byte encoding,
          // the same limit and the same reason as json.hpp:210-213.
          const int count = esc == 'x' ? 2 : (esc == 'u' ? 4 : 8);
          const int code  = hex_digits_(count);
          if(code > 0x7F)
          {
            throw std::runtime_error(
                std::format("YAML: \\{} escapes above 0x7F are not implemented", esc));
          }
          out.push_back(static_cast<char>(code));
          return;
        }
        default:
          throw std::runtime_error(std::format("YAML: unknown escape: \\{}", esc));
      }
    }
  };

  REFLEX_SERDE_DESERIALIZER_DEDUCTION_GUIDES(deserializer);

  // A block sequence, one "- " per entry.
  //
  // Two things make this harder than a block mapping.
  //
  // A sequence that is the value of a mapping key may sit at the key's OWN
  // indent, not deeper - both of these are the same document:
  //
  //   values:        values:
  //     - 1          - 1
  //     - 2          - 2
  //
  // so the "a child block must be strictly deeper" rule has an exception here.
  // It costs no extra state: a mapping never begins with "- ", so a sequence
  // entry at the enclosing indent can only be the flush form.
  //
  // The same collapse means the sequence cannot tell its own end from the
  // parent's next key by indent alone. A line at the sequence's indent that is
  // not an entry ends the sequence, rather than being an error.
  template <typename InputIt, seq_c Seq>
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Seq>)
  {
    const std::size_t base = de.next_content_line();
    if(base == de.npos)
    {
      return Seq{};
    }
    if(de.peek_at(0) == '[')
    {
      return de.template read_flow_sequence<Seq>();
    }
    if(not de.at_sequence_entry())
    {
      if(not de.deeper_than_block(base))
      {
        return Seq{}; // an absent value, and the cursor stays parked for the parent
      }
      throw std::runtime_error("YAML: expected '- ' at the start of a sequence");
    }

    Seq value{};
    using elem_type = typename std::remove_cvref_t<Seq>::value_type;

    auto push = serde::detail::make_pusher(
        value, "Sequence has more elements than target type can hold");

    auto scope = de.enter_block(base);

    while(true)
    {
      de.advance(); // '-'
      de.skip_separation();

      if(de.at_line_end())
      {
        // The entry's value is on the following lines, or there is none.
        const std::size_t inner = de.next_content_line();
        if(inner == de.npos or inner <= base)
        {
          push(elem_type{});
        }
        else
        {
          push(de.template load<elem_type>());
        }
      }
      else
      {
        push(de.template load<elem_type>()); // compact: the entry starts here
      }

      const std::size_t next = de.next_content_line();
      if(next == de.npos or next < base)
      {
        break;
      }
      if(next > base)
      {
        // Same causes as the mapping case below, minus the continuation: a plain
        // entry folds its deeper lines in itself, so anything still reaching here
        // is a form that cannot fold.
        throw std::runtime_error(
            "YAML: a line inside this sequence is indented deeper than its '- '. A plain"
            " entry continues onto deeper lines, so this line is one of the forms that"
            " cannot. Either it is misaligned, and belongs at the same indent as the other"
            " entries; or it looks like a mapping of its own, which real YAML rejects here"
            " too; or it continues a quoted scalar, which this backend reads on one line"
            " only. Use a '|' or '>' block scalar for a value that will not fold");
      }
      if(not de.at_sequence_entry())
      {
        break; // the flush form: a non-entry at this indent belongs to the parent
      }
    }
    return value;
  }

  // A block mapping into an aggregate or a poly::obj.
  //
  // Unlike JSON's, this loop has no delimiters to steer by (json.hpp:1056-1151
  // reads '{', ',' and '}'). The extent of a block mapping is its indentation:
  // it starts at the column of its first key, ends at the first content line
  // indented less, and a line indented more is an error rather than a
  // continuation.
  //
  template <typename InputIt, object_visitable_c Map>
    requires(
        not(meta::is_template_instance_of(^^Map, ^^poly::var)
            // std::array<char, N> may be considered a visitable object
            or str_c<Map>
            or seq_c<Map>))
  auto tag_invoke(
      tag_default_t<serde::deserialize>, deserializer<InputIt> & de, std::type_identity<Map>)
  {
    const std::size_t base = de.next_content_line();

    // Nothing here, or nothing deep enough to open a block: an absent value.
    // The cursor stays parked on whatever line ended the search, which is what
    // the enclosing loop is about to ask about.
    if(base == de.npos or not de.deeper_than_block(base))
    {
      return Map{};
    }

    if(de.peek_at(0) == '{')
    {
      return de.template read_flow_mapping<Map>();
    }

    Map  value{};
    auto scope = de.enter_block(base);

    while(true)
    {
      const std::string_view key = de.read_key();
      if(de.at_line_end() or de.peek_at(0) != ':')
      {
        throw std::runtime_error(std::format("YAML: expected ':' after the key '{}'", key));
      }
      de.advance(); // ':'
      de.skip_separation();

      serde::object_visit_flat(key, value, [&]<typename V>(V& v) {
        v = de.template load<std::remove_cvref_t<V>>();
      });

      // The value reader stopped at the end of its line, or - for a nested
      // block - on the first line it did not own. next_content_line() is
      // idempotent on the latter, which is what keeps this loop from skipping
      // the key that ended the nested block.
      const std::size_t next = de.next_content_line();
      if(next == de.npos or next < base)
      {
        break;
      }
      if(next > base)
      {
        // Several different documents land here and the message has to serve them
        // all. A plain value now folds its own deeper lines, so what is left is a
        // genuine misalignment, a deeper line that looks like a mapping - which
        // real YAML rejects too, for the same ambiguity - and the quoted and flow
        // forms this backend reads on one line only.
        throw std::runtime_error(
            "YAML: a line inside this mapping is indented deeper than its key. A plain"
            " value continues onto deeper lines, so this line is one of the forms that"
            " cannot. Either it is misaligned, and belongs at the same indent as the other"
            " keys; or it looks like a mapping of its own, which real YAML rejects here"
            " too; or it continues a quoted scalar, which this backend reads on one line"
            " only. Use a '|' or '>' block scalar for a value that will not fold");
      }
    }
    return value;
  }

} // namespace reflex::serde::yaml

REFLEX_EXPORT namespace reflex::serde::ser
{
  constexpr auto yaml = ^^reflex::serde::yaml::serializer;
}

REFLEX_EXPORT namespace reflex::serde::de
{
  constexpr auto yaml = ^^reflex::serde::yaml::deserializer;
}
